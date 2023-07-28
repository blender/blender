/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#ifdef __cplusplus
#  include <functional>
#  include <string>
#endif

#include "BLI_compiler_attrs.h"
#include "BLI_string_utf8_symbols.h"
#include "BLI_sys_types.h" /* size_t */
#include "BLI_utildefines.h"
#include "UI_interface_icons.h"
#include "WM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Struct Declarations */

struct ARegion;
struct AssetFilterSettings;
struct AssetRepresentation;
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
struct wmRegionListenerParams;
struct wmWindow;

typedef struct uiBlock uiBlock;
typedef struct uiBut uiBut;
typedef struct uiButExtraOpIcon uiButExtraOpIcon;
typedef struct uiLayout uiLayout;
typedef struct uiPopupBlockHandle uiPopupBlockHandle;
/* C handle for C++ #ui::AbstractView type. */
typedef struct uiViewHandle uiViewHandle;
/* C handle for C++ #ui::AbstractViewItem type. */
typedef struct uiViewItemHandle uiViewItemHandle;

/* Defines */

/**
 * Character used for splitting labels (right align text after this character).
 * Users should never see this character.
 * Only applied when #UI_BUT_HAS_SEP_CHAR flag is enabled, see it's doc-string for details.
 */
#define UI_SEP_CHAR '|'
#define UI_SEP_CHAR_S "|"

/**
 * Character used when value is indeterminate (multiple, unknown, unset).
 */
#define UI_VALUE_INDETERMINATE_CHAR BLI_STR_UTF8_EM_DASH

/**
 * Separator for text in search menus (right pointing arrow).
 * keep in sync with `string_search.cc`.
 */
#define UI_MENU_ARROW_SEP BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE

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

/** Use for clamping popups within the screen. */
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

/** #uiBlock::direction */
enum {
  UI_DIR_UP = 1 << 0,
  UI_DIR_DOWN = 1 << 1,
  UI_DIR_LEFT = 1 << 2,
  UI_DIR_RIGHT = 1 << 3,
  UI_DIR_CENTER_X = 1 << 4,
  UI_DIR_CENTER_Y = 1 << 5,

  UI_DIR_ALL = UI_DIR_UP | UI_DIR_DOWN | UI_DIR_LEFT | UI_DIR_RIGHT,
};

/** #uiBlock.flag (controls) */
enum {
  UI_BLOCK_LOOP = 1 << 0,
  /** Indicate that items in a popup are drawn with inverted order. Used for arrow key navigation
   *  so that it knows to invert the navigation direction to match the drawing order. */
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
  /** Stop handling mouse events. */
  UI_BLOCK_CLIP_EVENTS = 1 << 13,

  /* #uiBlock::flags bits 14-17 are identical to #uiBut::drawflag bits. */

  UI_BLOCK_POPUP_HOLD = 1 << 18,
  UI_BLOCK_LIST_ITEM = 1 << 19,
  UI_BLOCK_RADIAL = 1 << 20,
  UI_BLOCK_POPOVER = 1 << 21,
  UI_BLOCK_POPOVER_ONCE = 1 << 22,
  /** Always show key-maps, even for non-menus. */
  UI_BLOCK_SHOW_SHORTCUT_ALWAYS = 1 << 23,
  /** Don't show library override state for buttons in this block. */
  UI_BLOCK_NO_DRAW_OVERRIDDEN_STATE = 1 << 24,
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

/** #uiBut.flag general state flags. */
enum {
  /* WARNING: the first 8 flags are internal (see #UI_SELECT definition). */

  UI_BUT_ICON_SUBMENU = 1 << 8,
  UI_BUT_ICON_PREVIEW = 1 << 9,

  UI_BUT_NODE_LINK = 1 << 10,
  UI_BUT_NODE_ACTIVE = 1 << 11,
  UI_BUT_DRAG_LOCK = 1 << 12,
  /** Grayed out and un-editable. */
  UI_BUT_DISABLED = 1 << 13,

  UI_BUT_ANIMATED = 1 << 14,
  UI_BUT_ANIMATED_KEY = 1 << 15,
  UI_BUT_DRIVEN = 1 << 16,
  UI_BUT_REDALERT = 1 << 17,
  /** Grayed out but still editable. */
  UI_BUT_INACTIVE = 1 << 18,
  UI_BUT_LAST_ACTIVE = 1 << 19,
  UI_BUT_UNDO = 1 << 20,
  /* UNUSED = 1 << 21, */
  UI_BUT_NO_UTF8 = 1 << 22,

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

  /**
   * #uiBut.str contains #UI_SEP_CHAR, used to show key-shortcuts right aligned.
   *
   * Since a label may contain #UI_SEP_CHAR, it's important to split on the last occurrence
   * (meaning the right aligned text can't contain this character).
   */
  UI_BUT_HAS_SEP_CHAR = 1 << 27,
  /** Don't run updates while dragging (needed in rare cases). */
  UI_BUT_UPDATE_DELAY = 1 << 28,
  /** When widget is in text-edit mode, update value on each char stroke. */
  UI_BUT_TEXTEDIT_UPDATE = 1 << 29,
  /** Show 'x' icon to clear/unlink value of text or search button. */
  UI_BUT_VALUE_CLEAR = 1 << 30,

  /** RNA property of the button is overridden from linked reference data. */
  UI_BUT_OVERRIDDEN = 1u << 31u,
};

/** #uiBut.dragflag */
enum {
  /** By default only the left part of a button triggers dragging. A questionable design to make
   * the icon but not other parts of the button draggable. Set this flag so the entire button can
   * be dragged. */
  UI_BUT_DRAG_FULL_BUT = (1 << 0),

  /* --- Internal flags. --- */
  UI_BUT_DRAGPOIN_FREE = (1 << 1),
};

/** Default font size for normal text. */
#define UI_DEFAULT_TEXT_POINTS 11.0f

/** Larger size used for title text. */
#define UI_DEFAULT_TITLE_POINTS 11.0f

#define UI_PANEL_WIDTH 340
#define UI_COMPACT_PANEL_WIDTH 160
#define UI_SIDEBAR_PANEL_WIDTH 220
#define UI_NAVIGATION_REGION_WIDTH UI_COMPACT_PANEL_WIDTH
#define UI_NARROW_NAVIGATION_REGION_WIDTH 100

/** The width of one icon column of the Toolbar. */
#define UI_TOOLBAR_COLUMN (1.25f * ICON_DEFAULT_HEIGHT_TOOLBAR)
/** The space between the Toolbar and the area's edge. */
#define UI_TOOLBAR_MARGIN (0.5f * ICON_DEFAULT_HEIGHT_TOOLBAR)
/** Total width of Toolbar showing one icon column. */
#define UI_TOOLBAR_WIDTH UI_TOOLBAR_MARGIN + UI_TOOLBAR_COLUMN

#define UI_PANEL_CATEGORY_MARGIN_WIDTH (U.widget_unit * 1.0f)

/* Both these margins should be ignored if the panel doesn't show a background (check
 * #UI_panel_should_show_background()). */
#define UI_PANEL_MARGIN_X (U.widget_unit * 0.4f)
#define UI_PANEL_MARGIN_Y (U.widget_unit * 0.1f)

/**
 * #uiBut::drawflag, these flags should only affect how the button is drawn.
 *
 * \note currently, these flags *are not passed* to the widgets state() or draw() functions
 * (except for the 'align' ones)!
 */
enum {
  /** Text and icon alignment (by default, they are centered). */
  UI_BUT_TEXT_LEFT = 1 << 1,
  UI_BUT_ICON_LEFT = 1 << 2,
  UI_BUT_TEXT_RIGHT = 1 << 3,
  /** Prevent the button to show any tool-tip. */
  UI_BUT_NO_TOOLTIP = 1 << 4,
  /**
   * Show a quick tool-tip label, that is, a short tool-tip that appears faster than the full one
   * and only shows the label. After a short delay the full tool-tip is shown if any.
   */
  UI_BUT_HAS_TOOLTIP_LABEL = 1 << 5,
  /** Do not add the usual horizontal padding for text drawing. */
  UI_BUT_NO_TEXT_PADDING = 1 << 6,

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
   * because of "corrective" hack in #widget_roundbox_set().
   */
  UI_BUT_ALIGN_STITCH_TOP = 1 << 18,
  UI_BUT_ALIGN_STITCH_LEFT = 1 << 19,
  UI_BUT_ALIGN_ALL = UI_BUT_ALIGN | UI_BUT_ALIGN_STITCH_TOP | UI_BUT_ALIGN_STITCH_LEFT,

  /** This but is "inside" a box item (currently used to change theme colors). */
  UI_BUT_BOX_ITEM = 1 << 20,

  /** Active left part of number button */
  UI_BUT_ACTIVE_LEFT = 1 << 21,
  /** Active right part of number button */
  UI_BUT_ACTIVE_RIGHT = 1 << 22,

  /** Reverse order of consecutive off/on icons */
  UI_BUT_ICON_REVERSE = 1 << 23,

  /** Value is animated, but the current value differs from the animated one. */
  UI_BUT_ANIMATED_CHANGED = 1 << 24,

  /** Draw the checkbox buttons inverted. */
  UI_BUT_CHECKBOX_INVERT = 1 << 25,

  /** Drawn in a way that indicates that the state/value is unknown. */
  UI_BUT_INDETERMINATE = 1 << 26,
};

/**
 * Button types, bits stored in 1 value... and a short even!
 * - bits 0-4:  #uiBut.bitnr (0-31)
 * - bits 5-7:  pointer type
 * - bit  8:    for 'bit'
 * - bit  9-15: button type (now 6 bits, 64 types)
 */
typedef enum {
  UI_BUT_POIN_NONE = 0,

  UI_BUT_POIN_CHAR = 32,
  UI_BUT_POIN_SHORT = 64,
  UI_BUT_POIN_INT = 96,
  UI_BUT_POIN_FLOAT = 128,
  // UI_BUT_POIN_FUNCTION = 192, /* UNUSED */
  UI_BUT_POIN_BIT = 256, /* OR'd with a bit index. */
} eButPointerType;

/** \note requires `but->poin != NULL`. */
#define UI_BUT_POIN_TYPES (UI_BUT_POIN_FLOAT | UI_BUT_POIN_SHORT | UI_BUT_POIN_CHAR)

/**
 * #uiBut::type
 * OR'd with #eButPointerType when passing as an argument.
 */
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
  /** Menu (often used in headers), `*_MENU` with different draw-type. */
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
  /** A preview image (#PreviewImage), with text under it. Typically bigger than normal buttons and
   * laid out in a grid, e.g. like the File Browser in thumbnail display mode. */
  UI_BTYPE_PREVIEW_TILE = 43 << 9,
  UI_BTYPE_HOTKEY_EVENT = 46 << 9,
  /** Non-interactive image, used for splash screen */
  UI_BTYPE_IMAGE = 47 << 9,
  UI_BTYPE_HISTOGRAM = 48 << 9,
  UI_BTYPE_WAVEFORM = 49 << 9,
  UI_BTYPE_VECTORSCOPE = 50 << 9,
  UI_BTYPE_PROGRESS = 51 << 9,
  UI_BTYPE_NODE_SOCKET = 53 << 9,
  UI_BTYPE_SEPR = 54 << 9,
  UI_BTYPE_SEPR_LINE = 55 << 9,
  /** Dynamically fill available space. */
  UI_BTYPE_SEPR_SPACER = 56 << 9,
  /** Resize handle (resize UI-list). */
  UI_BTYPE_GRIP = 57 << 9,
  UI_BTYPE_DECORATOR = 58 << 9,
  /** An item a view (see #ui::AbstractViewItem). */
  UI_BTYPE_VIEW_ITEM = 59 << 9,
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

/**
 * Draw title and text safe areas.
 *
 * \note This function is to be used with the 2D dashed shader enabled.
 *
 * \param pos: is a #PRIM_FLOAT, 2, #GPU_FETCH_FLOAT vertex attribute.
 * \param rect: The offsets for the view, not the zones.
 */
void UI_draw_safe_areas(uint pos,
                        const struct rctf *rect,
                        const float title_aspect[2],
                        const float action_aspect[2]);

/** State for scroll-drawing. */
enum {
  UI_SCROLL_PRESSED = 1 << 0,
  UI_SCROLL_ARROWS = 1 << 1,
};
/**
 * Function in use for buttons and for view2d sliders.
 */
void UI_draw_widget_scroll(struct uiWidgetColors *wcol,
                           const struct rcti *rect,
                           const struct rcti *slider,
                           int state);

/**
 * Shortening string helper.
 *
 * Cut off the middle of the text to fit into the given width.
 *
 * \note in case this middle clipping would just remove a few chars,
 * it rather clips right, which is more readable.
 *
 * If `rpart_sep` is not null, the part of `str` starting to first occurrence of `rpart_sep`
 * is preserved at all cost.
 * Useful for strings with shortcuts
 * (like `A Very Long Foo Bar Label For Menu Entry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O`).
 */
float UI_text_clip_middle_ex(const struct uiFontStyle *fstyle,
                             char *str,
                             float okwidth,
                             float minwidth,
                             size_t max_len,
                             char rpart_sep);

/**
 * Callbacks.
 *
 * #UI_block_func_handle_set/ButmFunc are for handling events through a callback.
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

/** Function to compare the identity of two buttons over redraws, to check if they represent the
 * same data, and thus should be considered the same button over redraws. */
typedef bool (*uiButIdentityCompareFunc)(const uiBut *a, const uiBut *b);

/* Search types. */
typedef struct ARegion *(*uiButSearchCreateFn)(struct bContext *C,
                                               struct ARegion *butregion,
                                               struct uiButSearch *search_but);
/**
 * `is_first` is typically used to ignore search filtering when the menu is first opened in order
 * to display the full list of options. The value will be false after the button's text is edited
 * (for every call except the first).
 */
typedef void (*uiButSearchUpdateFn)(
    const struct bContext *C, void *arg, const char *str, uiSearchItems *items, bool is_first);
typedef bool (*uiButSearchContextMenuFn)(struct bContext *C,
                                         void *arg,
                                         void *active,
                                         const struct wmEvent *event);
typedef struct ARegion *(*uiButSearchTooltipFn)(struct bContext *C,
                                                struct ARegion *region,
                                                const struct rcti *item_rect,
                                                void *arg,
                                                void *active);
typedef void (*uiButSearchListenFn)(const struct wmRegionListenerParams *params, void *arg);

/** Must return an allocated string. */
typedef char *(*uiButToolTipFunc)(struct bContext *C, void *argN, const char *tip);

typedef void (*uiBlockHandleFunc)(struct bContext *C, void *arg, int event);

/* -------------------------------------------------------------------- */
/** \name Custom Interaction
 *
 * Sometimes it's useful to create data that remains available
 * while the user interacts with a button.
 *
 * A common case is dragging a number button or slider
 * however this could be used in other cases too.
 * \{ */

struct uiBlockInteraction_Params {
  /**
   * When true, this interaction is not modal
   * (user clicking on a number button arrows or pasting a value for example).
   */
  bool is_click;
  /**
   * Array of unique event ID's (values from #uiBut.retval).
   * There may be more than one for multi-button editing (see #UI_BUT_DRAG_MULTI).
   */
  int *unique_retval_ids;
  uint unique_retval_ids_len;
};

/** Returns 'user_data', freed by #uiBlockInteractionEndFn. */
typedef void *(*uiBlockInteractionBeginFn)(struct bContext *C,
                                           const struct uiBlockInteraction_Params *params,
                                           void *arg1);
typedef void (*uiBlockInteractionEndFn)(struct bContext *C,
                                        const struct uiBlockInteraction_Params *params,
                                        void *arg1,
                                        void *user_data);
typedef void (*uiBlockInteractionUpdateFn)(struct bContext *C,
                                           const struct uiBlockInteraction_Params *params,
                                           void *arg1,
                                           void *user_data);

typedef struct uiBlockInteraction_CallbackData {
  uiBlockInteractionBeginFn begin_fn;
  uiBlockInteractionEndFn end_fn;
  uiBlockInteractionUpdateFn update_fn;
  void *arg1;
} uiBlockInteraction_CallbackData;

void UI_block_interaction_set(uiBlock *block, uiBlockInteraction_CallbackData *callbacks);

/** \} */

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

typedef void *(*uiCopyArgFunc)(const void *arg);
typedef void (*uiFreeArgFunc)(void *arg);

/* interface_query.c */

bool UI_but_has_tooltip_label(const uiBut *but);
bool UI_but_is_tool(const uiBut *but);
/* file selectors are exempt from utf-8 checks */
bool UI_but_is_utf8(const uiBut *but);
#define UI_but_is_decorator(but) ((but)->type == UI_BTYPE_DECORATOR)

bool UI_block_is_empty_ex(const uiBlock *block, bool skip_title);
bool UI_block_is_empty(const uiBlock *block);
bool UI_block_can_add_separator(const uiBlock *block);

struct uiList *UI_list_find_mouse_over(const struct ARegion *region, const struct wmEvent *event);

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
/**
 * Directly create a popup menu that is not refreshed on redraw.
 *
 * Only return handler, and set optional title.
 * \param block_name: Assigned to uiBlock.name (useful info for debugging).
 */
uiPopupMenu *UI_popup_menu_begin_ex(struct bContext *C,
                                    const char *title,
                                    const char *block_name,
                                    int icon) ATTR_NONNULL();
/**
 * Set the whole structure to work.
 */
void UI_popup_menu_end(struct bContext *C, struct uiPopupMenu *pup);
bool UI_popup_menu_end_or_cancel(struct bContext *C, struct uiPopupMenu *pup);
struct uiLayout *UI_popup_menu_layout(uiPopupMenu *pup);

void UI_popup_menu_reports(struct bContext *C, struct ReportList *reports) ATTR_NONNULL();
int UI_popup_menu_invoke(struct bContext *C, const char *idname, struct ReportList *reports)
    ATTR_NONNULL(1, 2);

/**
 * Allow setting menu return value from externals.
 * E.g. WM might need to do this for exiting files correctly.
 */
void UI_popup_menu_retval_set(const uiBlock *block, int retval, bool enable);
/**
 * Setting the button makes the popup open from the button instead of the cursor.
 */
void UI_popup_menu_but_set(uiPopupMenu *pup, struct ARegion *butregion, uiBut *but);

/* interface_region_popover.c */

typedef struct uiPopover uiPopover;

int UI_popover_panel_invoke(struct bContext *C,
                            const char *idname,
                            bool keep_open,
                            struct ReportList *reports);

/**
 * Only return handler, and set optional title.
 *
 * \param from_active_button: Use the active button for positioning,
 * use when the popover is activated from an operator instead of directly from the button.
 */
uiPopover *UI_popover_begin(struct bContext *C, int menu_width, bool from_active_button)
    ATTR_NONNULL(1);
/**
 * Set the whole structure to work.
 */
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
                           uiFreeArgFunc arg_free);
void UI_popup_block_invoke_ex(struct bContext *C,
                              uiBlockCreateFunc func,
                              void *arg,
                              uiFreeArgFunc arg_free,
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
                        wmOperatorCallContext opcontext);
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
/**
 * Uses local copy of style, to scale things down, and allow widgets to change stuff.
 */
void UI_block_draw(const struct bContext *C, struct uiBlock *block);
void UI_blocklist_update_window_matrix(const struct bContext *C, const struct ListBase *lb);
void UI_blocklist_update_view_for_buttons(const struct bContext *C, const struct ListBase *lb);
void UI_blocklist_draw(const struct bContext *C, const struct ListBase *lb);
void UI_block_update_from_old(const struct bContext *C, struct uiBlock *block);

enum {
  UI_BLOCK_THEME_STYLE_REGULAR = 0,
  UI_BLOCK_THEME_STYLE_POPUP = 1,
};
void UI_block_theme_style_set(uiBlock *block, char theme_style);
eUIEmbossType UI_block_emboss_get(uiBlock *block);
void UI_block_emboss_set(uiBlock *block, eUIEmbossType emboss);
bool UI_block_is_search_only(const uiBlock *block);
/**
 * Use when a block must be searched to give accurate results
 * for the whole region but shouldn't be displayed.
 */
void UI_block_set_search_only(uiBlock *block, bool search_only);

/**
 * Can be called with C==NULL.
 */
void UI_block_free(const struct bContext *C, uiBlock *block);

void UI_block_listen(const uiBlock *block, const struct wmRegionListenerParams *listener_params);

/**
 * Can be called with C==NULL.
 */
void UI_blocklist_free(const struct bContext *C, struct ARegion *region);
void UI_blocklist_free_inactive(const struct bContext *C, struct ARegion *region);

/**
 * Is called by notifier.
 */
void UI_screen_free_active_but_highlight(const struct bContext *C, struct bScreen *screen);
void UI_region_free_active_but_all(struct bContext *context, struct ARegion *region);

void UI_block_region_set(uiBlock *block, struct ARegion *region);

void UI_block_lock_set(uiBlock *block, bool val, const char *lockstr);
void UI_block_lock_clear(uiBlock *block);

/**
 * Automatic aligning, horizontal or vertical.
 */
void UI_block_align_begin(uiBlock *block);
void UI_block_align_end(uiBlock *block);

/** Block bounds/position calculation. */
typedef enum {
  UI_BLOCK_BOUNDS_NONE = 0,
  UI_BLOCK_BOUNDS = 1,
  UI_BLOCK_BOUNDS_TEXT,
  UI_BLOCK_BOUNDS_POPUP_MOUSE,
  UI_BLOCK_BOUNDS_POPUP_MENU,
  UI_BLOCK_BOUNDS_POPUP_CENTER,
  UI_BLOCK_BOUNDS_PIE_CENTER,
} eBlockBoundsCalc;

/**
 * Used for various cases.
 */
void UI_block_bounds_set_normal(struct uiBlock *block, int addval);
/**
 * Used for pull-downs.
 */
void UI_block_bounds_set_text(uiBlock *block, int addval);
/**
 * Used for block popups.
 */
void UI_block_bounds_set_popup(uiBlock *block, int addval, const int bounds_offset[2]);
/**
 * Used for menu popups.
 */
void UI_block_bounds_set_menu(uiBlock *block, int addval, const int bounds_offset[2]);
/**
 * Used for centered popups, i.e. splash.
 */
void UI_block_bounds_set_centered(uiBlock *block, int addval);
void UI_block_bounds_set_explicit(uiBlock *block, int minx, int miny, int maxx, int maxy);

int UI_blocklist_min_y_get(struct ListBase *lb);

void UI_block_direction_set(uiBlock *block, char direction);
/**
 * This call escapes if there's alignment flags.
 */
void UI_block_order_flip(uiBlock *block);
void UI_block_flag_enable(uiBlock *block, int flag);
void UI_block_flag_disable(uiBlock *block, int flag);
void UI_block_translate(uiBlock *block, int x, int y);

int UI_but_return_value_get(uiBut *but);

uiBut *UI_but_active_drop_name_button(const struct bContext *C);
/**
 * Returns true if highlighted button allows drop of names.
 * called in region context.
 */
bool UI_but_active_drop_name(const struct bContext *C);
bool UI_but_active_drop_color(struct bContext *C);

void UI_but_flag_enable(uiBut *but, int flag);
void UI_but_flag_disable(uiBut *but, int flag);
bool UI_but_flag_is_set(uiBut *but, int flag);

void UI_but_drawflag_enable(uiBut *but, int flag);
void UI_but_drawflag_disable(uiBut *but, int flag);

void UI_but_dragflag_enable(uiBut *but, int flag);
void UI_but_dragflag_disable(uiBut *but, int flag);

void UI_but_disable(uiBut *but, const char *disabled_hint);

void UI_but_type_set_menu_from_pulldown(uiBut *but);

/**
 * Special button case, only draw it when used actively, for outliner etc.
 *
 * Needed for temporarily rename buttons, such as in outliner or file-select,
 * they should keep calling #uiDefBut to keep them alive.
 * \return false when button removed.
 */
bool UI_but_active_only_ex(const struct bContext *C,
                           struct ARegion *region,
                           uiBlock *block,
                           uiBut *but,
                           bool remove_on_failure);
bool UI_but_active_only(const struct bContext *C,
                        struct ARegion *region,
                        uiBlock *block,
                        uiBut *but);
/**
 * \warning This must run after other handlers have been added,
 * otherwise the handler won't be removed, see: #71112.
 */
bool UI_block_active_only_flagged_buttons(const struct bContext *C,
                                          struct ARegion *region,
                                          struct uiBlock *block);

/**
 * Simulate button click.
 */
void UI_but_execute(const struct bContext *C, struct ARegion *region, uiBut *but);

bool UI_but_online_manual_id(const uiBut *but,
                             char *r_str,
                             size_t str_maxncpy) ATTR_WARN_UNUSED_RESULT;
bool UI_but_online_manual_id_from_active(const struct bContext *C,
                                         char *r_str,
                                         size_t str_maxncpy) ATTR_WARN_UNUSED_RESULT;
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
                 wmOperatorCallContext opcontext,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 const char *tip);
uiBut *uiDefButO_ptr(uiBlock *block,
                     int type,
                     struct wmOperatorType *ot,
                     wmOperatorCallContext opcontext,
                     const char *str,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip);

/**
 * If a1==1.0 then a2 is an extra icon blending factor (alpha 0.0 - 1.0).
 */
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
                     wmOperatorCallContext opcontext,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip);
uiBut *uiDefIconButO_ptr(uiBlock *block,
                         int type,
                         struct wmOperatorType *ot,
                         wmOperatorCallContext opcontext,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip);
uiBut *uiDefButImage(
    uiBlock *block, void *imbuf, int x, int y, short width, short height, const uchar color[4]);
uiBut *uiDefButAlert(uiBlock *block, int icon, int x, int y, short width, short height);
/** Button containing both string label and icon. */
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
                         wmOperatorCallContext opcontext,
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
                             wmOperatorCallContext opcontext,
                             int icon,
                             const char *str,
                             int x,
                             int y,
                             short width,
                             short height,
                             const char *tip);

/** For passing inputs to ButO buttons. */
struct PointerRNA *UI_but_operator_ptr_get(uiBut *but);

void UI_but_context_ptr_set(uiBlock *block,
                            uiBut *but,
                            const char *name,
                            const struct PointerRNA *ptr);
const struct PointerRNA *UI_but_context_ptr_get(const uiBut *but,
                                                const char *name,
                                                const StructRNA *type CPP_ARG_DEFAULT(nullptr));
struct bContextStore *UI_but_context_get(const uiBut *but);

void UI_but_unit_type_set(uiBut *but, int unit_type);
int UI_but_unit_type_get(const uiBut *but);

typedef enum uiStringInfoType {
  BUT_GET_RNAPROP_IDENTIFIER = 1,
  BUT_GET_RNASTRUCT_IDENTIFIER,
  BUT_GET_RNAENUM_IDENTIFIER,
  BUT_GET_LABEL,
  /** Sometimes the button doesn't have a label itself, but provides one for the tooltip. This can
   * be displayed in a quick tooltip, appearing after a smaller timeout and expanding to the full
   * tooltip after the regular timeout. */
  BUT_GET_TIP_LABEL,
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

/**
 * \note Expects pointers to #uiStringInfo structs as parameters.
 * Will fill them with translated strings, when possible.
 * Strings in #uiStringInfo must be MEM_freeN'ed by caller.
 */
void UI_but_string_info_get(struct bContext *C, uiBut *but, ...) ATTR_SENTINEL(0);
void UI_but_extra_icon_string_info_get(struct bContext *C, uiButExtraOpIcon *extra_icon, ...)
    ATTR_SENTINEL(0);

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
  UI_ID_NOP = 0,
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

enum eButProgressType {
  UI_BUT_PROGRESS_TYPE_BAR = 0,
  UI_BUT_PROGRESS_TYPE_RING = 1,
};

/***************************** ID Utilities *******************************/

int UI_icon_from_id(const struct ID *id);
/** See: #BKE_report_type_str */
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

/**
 * Block button containing icon.
 */
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
/**
 * Block button containing both string label and icon.
 */
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

/**
 * \param arg: A pointer to string/name, use #UI_but_func_search_set() below to make this work.
 * here `a1` and `a2`, if set, control thumbnail preview rows/cols.
 */
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
/**
 * Same parameters as for #uiDefSearchBut, with additional operator type and properties,
 * used by callback to call again the right op with the right options (properties values).
 */
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

/** For #uiDefAutoButsRNA. */
typedef enum {
  /** Keep current layout for aligning label with property button. */
  UI_BUT_LABEL_ALIGN_NONE,
  /** Align label and property button vertically. */
  UI_BUT_LABEL_ALIGN_COLUMN,
  /** Split layout into a column for the label and one for property button. */
  UI_BUT_LABEL_ALIGN_SPLIT_COLUMN,
} eButLabelAlign;

/** Return info for uiDefAutoButsRNA. */
typedef enum eAutoPropButsReturn {
  /** Returns when no buttons were added */
  UI_PROP_BUTS_NONE_ADDED = 1 << 0,
  /** Returned when any property failed the custom check callback (check_prop) */
  UI_PROP_BUTS_ANY_FAILED_CHECK = 1 << 1,
} eAutoPropButsReturn;

ENUM_OPERATORS(eAutoPropButsReturn, UI_PROP_BUTS_ANY_FAILED_CHECK);

uiBut *uiDefAutoButR(uiBlock *block,
                     struct PointerRNA *ptr,
                     struct PropertyRNA *prop,
                     int index,
                     const char *name,
                     int icon,
                     int x,
                     int y,
                     int width,
                     int height);
void uiDefAutoButsArrayR(uiBlock *block,
                         PointerRNA *ptr,
                         PropertyRNA *prop,
                         const int icon,
                         const int x,
                         const int y,
                         const int tot_width,
                         const int height);
/**
 * \a check_prop callback filters functions to avoid drawing certain properties,
 * in cases where PROP_HIDDEN flag can't be used for a property.
 *
 * \param prop_activate_init: Property to activate on initial popup (#UI_BUT_ACTIVATE_ON_INIT).
 */
eAutoPropButsReturn uiDefAutoButsRNA(uiLayout *layout,
                                     struct PointerRNA *ptr,
                                     bool (*check_prop)(struct PointerRNA *ptr,
                                                        struct PropertyRNA *prop,
                                                        void *user_data),
                                     void *user_data,
                                     struct PropertyRNA *prop_activate_init,
                                     eButLabelAlign label_align,
                                     bool compact);

/**
 * Callback to compare the identity of two buttons, used to identify buttons over redraws. If the
 * callback returns true, the given buttons are considered to be matching and relevant state is
 * preserved (copied from the old to the new button). If it returns false, it's considered
 * non-matching and no further checks are done.
 *
 * If this is set, it is always executed instead of the default comparisons. However it is only
 * executed for buttons that have the same type and the same callback. So callbacks can assume the
 * button types match.
 */
void UI_but_func_identity_compare_set(uiBut *but, uiButIdentityCompareFunc cmp_fn);

/**
 * Public function exported for functions that use #UI_BTYPE_SEARCH_MENU.
 *
 * Use inside searchfunc to add items.
 *
 * \param items: Stores the items.
 * \param name: Text to display for the item.
 * \param poin: Opaque pointer (for use by the caller).
 * \param iconid: The icon, #ICON_NONE for no icon.
 * \param but_flag: Button flags (#uiBut.flag) indicating the state of the item, typically
 *                  #UI_BUT_DISABLED, #UI_BUT_INACTIVE or #UI_BUT_HAS_SEP_CHAR.
 *
 * \return false if there is nothing to add.
 */
bool UI_search_item_add(uiSearchItems *items,
                        const char *name,
                        void *poin,
                        int iconid,
                        int but_flag,
                        uint8_t name_prefix_offset);

/**
 * \note The item-pointer (referred to below) is a per search item user pointer
 * passed to #UI_search_item_add (stored in  #uiSearchItems.pointers).
 *
 * \param search_create_fn: Function to create the menu.
 * \param search_update_fn: Function to refresh search content after the search text has changed.
 * \param arg: user value.
 * \param free_arg: Set to true if the argument is newly allocated memory for every redraw and
 * should be freed when the button is destroyed.
 * \param search_arg_free_fn: When non-null, use this function to free \a arg.
 * \param search_exec_fn: Function that executes the action, gets \a arg as the first argument.
 * The second argument as the active item-pointer
 * \param active: When non-null, this item-pointer item will be visible and selected,
 * otherwise the first item will be selected.
 */
void UI_but_func_search_set(uiBut *but,
                            uiButSearchCreateFn search_create_fn,
                            uiButSearchUpdateFn search_update_fn,
                            void *arg,
                            bool free_arg,
                            uiFreeArgFunc search_arg_free_fn,
                            uiButHandleFunc search_exec_fn,
                            void *active);
void UI_but_func_search_set_context_menu(uiBut *but, uiButSearchContextMenuFn context_menu_fn);
void UI_but_func_search_set_tooltip(uiBut *but, uiButSearchTooltipFn tooltip_fn);
void UI_but_func_search_set_listen(uiBut *but, uiButSearchListenFn listen_fn);
/**
 * \param search_sep_string: when not NULL, this string is used as a separator,
 * showing the icon and highlighted text after the last instance of this string.
 */
void UI_but_func_search_set_sep_string(uiBut *but, const char *search_sep_string);
void UI_but_func_search_set_results_are_suggestions(uiBut *but, bool value);

/**
 * Height in pixels, it's using hard-coded values still.
 */
int UI_searchbox_size_y(void);
int UI_searchbox_size_x(void);
/**
 * Check if a string is in an existing search box.
 */
int UI_search_items_find_index(uiSearchItems *items, const char *name);

/**
 * Adds a hint to the button which draws right aligned, grayed out and never clipped.
 */
void UI_but_hint_drawstr_set(uiBut *but, const char *string);
void UI_but_icon_indicator_number_set(uiBut *but, const int indicator_number);

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

void UI_but_func_tooltip_set(uiBut *but, uiButToolTipFunc func, void *arg, uiFreeArgFunc free_arg);
#ifdef __cplusplus
void UI_but_func_tooltip_label_set(uiBut *but, std::function<std::string(const uiBut *but)> func);
#endif
/**
 * Recreate tool-tip (use to update dynamic tips)
 */
void UI_but_tooltip_refresh(struct bContext *C, uiBut *but);
/**
 * Removes tool-tip timer from active but
 * (meaning tool-tip is disabled until it's re-enabled again).
 */
void UI_but_tooltip_timer_remove(struct bContext *C, uiBut *but);

bool UI_textbutton_activate_rna(const struct bContext *C,
                                struct ARegion *region,
                                const void *rna_poin_data,
                                const char *rna_prop_id);
bool UI_textbutton_activate_but(const struct bContext *C, uiBut *but);

/**
 * push a new event onto event queue to activate the given button
 * (usually a text-field) upon entering a popup
 */
void UI_but_focus_on_enter_event(struct wmWindow *win, uiBut *but);

void UI_but_func_hold_set(uiBut *but, uiButHandleHoldFunc func, void *argN);

struct PointerRNA *UI_but_extra_operator_icon_add(uiBut *but,
                                                  const char *opname,
                                                  wmOperatorCallContext opcontext,
                                                  int icon);
struct wmOperatorType *UI_but_extra_operator_icon_optype_get(struct uiButExtraOpIcon *extra_icon);
struct PointerRNA *UI_but_extra_operator_icon_opptr_get(struct uiButExtraOpIcon *extra_icon);

/**
 * Get the scaled size for a preview button (typically #UI_BTyPE_PREVIEW_TILE) based on \a
 * size_px plus padding.
 */
int UI_preview_tile_size_x(const int size_px CPP_ARG_DEFAULT(96));
int UI_preview_tile_size_y(const int size_px CPP_ARG_DEFAULT(96));
int UI_preview_tile_size_y_no_label(const int size_px CPP_ARG_DEFAULT(96));

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

/* Button drag-data (interface_drag.cc).
 *
 * Functions to set drag data for buttons. This enables dragging support, whereby the drag data is
 * "dragged", not the button itself. */

void UI_but_drag_set_id(uiBut *but, struct ID *id);
/**
 * Set an image to display while dragging. This works for any drag type (`WM_DRAG_XXX`).
 * Not to be confused with #UI_but_drag_set_image(), which sets up dragging of an image.
 *
 * Sets #UI_BUT_DRAG_FULL_BUT so the full button can be dragged.
 */
void UI_but_drag_attach_image(uiBut *but, const struct ImBuf *imb, float scale);

#ifdef __cplusplus
/**
 * Sets #UI_BUT_DRAG_FULL_BUT so the full button can be dragged.
 * \param asset: May be passed from a temporary variable, drag data only stores a copy of this.
 */
void UI_but_drag_set_asset(uiBut *but,
                           const blender::asset_system::AssetRepresentation *asset,
                           int import_type, /* eAssetImportType */
                           int icon,
                           const struct ImBuf *imb,
                           float scale);
#endif

void UI_but_drag_set_rna(uiBut *but, struct PointerRNA *ptr);
/**
 * Enable dragging a path from this button.
 * \param path: The path to drag. The passed string may be destructed, button keeps a copy.
 */
void UI_but_drag_set_path(uiBut *but, const char *path);
void UI_but_drag_set_name(uiBut *but, const char *name);
/**
 * Value from button itself.
 */
void UI_but_drag_set_value(uiBut *but);

/**
 * Sets #UI_BUT_DRAG_FULL_BUT so the full button can be dragged.
 * \param path: The path to drag. The passed string may be destructed, button keeps a copy.
 */
void UI_but_drag_set_image(
    uiBut *but, const char *path, int icon, const struct ImBuf *imb, float scale);

/* Panels
 *
 * Functions for creating, freeing and drawing panels. The API here
 * could use a good cleanup, though how they will function in 2.5 is
 * not clear yet so we postpone that. */

void UI_panels_begin(const struct bContext *C, struct ARegion *region);
void UI_panels_end(const struct bContext *C, struct ARegion *region, int *r_x, int *r_y);
/**
 * Draw panels, selected (panels currently being dragged) on top.
 */
void UI_panels_draw(const struct bContext *C, struct ARegion *region);

struct Panel *UI_panel_find_by_type(struct ListBase *lb, const struct PanelType *pt);
/**
 * \note \a panel should be return value from #UI_panel_find_by_type and can be NULL.
 */
struct Panel *UI_panel_begin(struct ARegion *region,
                             struct ListBase *lb,
                             uiBlock *block,
                             struct PanelType *pt,
                             struct Panel *panel,
                             bool *r_open);
/**
 * Create the panel header button group, used to mark which buttons are part of
 * panel headers for the panel search process that happens later. This Should be
 * called before adding buttons for the panel's header layout.
 */
void UI_panel_header_buttons_begin(struct Panel *panel);
/**
 * Finish the button group for the panel header to avoid putting panel body buttons in it.
 */
void UI_panel_header_buttons_end(struct Panel *panel);
void UI_panel_end(struct Panel *panel, int width, int height);

/**
 * Set a context for this entire panel and its current layout. This should be used whenever panel
 * callbacks that are called outside of regular drawing might require context. Currently it affects
 * the #PanelType.reorder callback only.
 */
void UI_panel_context_pointer_set(struct Panel *panel, const char *name, struct PointerRNA *ptr);

/**
 * Get the panel's expansion state, taking into account
 * expansion set from property search if it applies.
 */
bool UI_panel_is_closed(const struct Panel *panel);
bool UI_panel_is_active(const struct Panel *panel);
/**
 * For button layout next to label.
 */
void UI_panel_label_offset(const struct uiBlock *block, int *r_x, int *r_y);
bool UI_panel_should_show_background(const struct ARegion *region,
                                     const struct PanelType *panel_type);
int UI_panel_size_y(const struct Panel *panel);
bool UI_panel_is_dragging(const struct Panel *panel);
/**
 * Find whether a panel or any of its sub-panels contain a property that matches the search filter,
 * depending on the search process running in #UI_block_apply_search_filter earlier.
 */
bool UI_panel_matches_search_filter(const struct Panel *panel);
bool UI_panel_can_be_pinned(const struct Panel *panel);

bool UI_panel_category_is_visible(const struct ARegion *region);
void UI_panel_category_add(struct ARegion *region, const char *name);
struct PanelCategoryDyn *UI_panel_category_find(const struct ARegion *region, const char *idname);
struct PanelCategoryStack *UI_panel_category_active_find(struct ARegion *region,
                                                         const char *idname);
const char *UI_panel_category_active_get(struct ARegion *region, bool set_fallback);
void UI_panel_category_active_set(struct ARegion *region, const char *idname);
void UI_panel_category_active_set_default(struct ARegion *region, const char *idname);
void UI_panel_category_clear_all(struct ARegion *region);
/**
 * Draw vertical tabs on the left side of the region, one tab per category.
 */
void UI_panel_category_draw_all(struct ARegion *region, const char *category_id_active);

/* Panel custom data. */
struct PointerRNA *UI_panel_custom_data_get(const struct Panel *panel);
struct PointerRNA *UI_region_panel_custom_data_under_cursor(const struct bContext *C,
                                                            const struct wmEvent *event);
void UI_panel_custom_data_set(struct Panel *panel, struct PointerRNA *custom_data);

/* Poly-instantiated panels for representing a list of data. */
/**
 * Called in situations where panels need to be added dynamically rather than
 * having only one panel corresponding to each #PanelType.
 */
struct Panel *UI_panel_add_instanced(const struct bContext *C,
                                     struct ARegion *region,
                                     struct ListBase *panels,
                                     const char *panel_idname,
                                     struct PointerRNA *custom_data);
/**
 * Remove instanced panels from the region's panel list.
 *
 * \note Can be called with NULL \a C, but it should be avoided because
 * handlers might not be removed.
 */
void UI_panels_free_instanced(const struct bContext *C, struct ARegion *region);

#define INSTANCED_PANEL_UNIQUE_STR_SIZE 16
/**
 * Find a unique key to append to the #PanelType.idname for the lookup to the panel's #uiBlock.
 * Needed for instanced panels, where there can be multiple with the same type and identifier.
 */
void UI_list_panel_unique_str(struct Panel *panel, char *r_name);

typedef void (*uiListPanelIDFromDataFunc)(void *data_link, char *r_idname);
/**
 * Check if the instanced panels in the region's panels correspond to the list of data the panels
 * represent. Returns false if the panels have been reordered or if the types from the list data
 * don't match in any way.
 *
 * \param data: The list of data to check against the instanced panels.
 * \param panel_idname_func: Function to find the #PanelType.idname for each item in the data list.
 * For a readability and generality, this lookup happens separately for each type of panel list.
 */
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
                           char flag);
void UI_popup_handlers_remove(struct ListBase *handlers, uiPopupBlockHandle *popup);
void UI_popup_handlers_remove_all(struct bContext *C, struct ListBase *handlers);

/* Module
 *
 * init and exit should be called before using this module. init_userdef must
 * be used to reinitialize some internal state if user preferences change. */

void UI_init(void);
/* after reading userdef file */
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

/* Used for transparent checkers shown under color buttons that have an alpha component. */
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
bool UI_block_layout_needs_resolving(const uiBlock *block);
/**
 * Used for property search when the layout process needs to be cancelled in order to avoid
 * computing the locations for buttons, but the layout items created while adding the buttons
 * must still be freed.
 */
void UI_block_layout_free(uiBlock *block);

/**
 * Apply property search behavior, setting panel flags and deactivating buttons that don't match.
 *
 * \note Must not be run after #UI_block_layout_resolve.
 */
bool UI_block_apply_search_filter(uiBlock *block, const char *search_filter);

void UI_region_message_subscribe(struct ARegion *region, struct wmMsgBus *mbus);

uiBlock *uiLayoutGetBlock(uiLayout *layout);

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv);
void uiLayoutSetContextPointer(uiLayout *layout, const char *name, struct PointerRNA *ptr);
struct bContextStore *uiLayoutGetContextStore(uiLayout *layout);
void uiLayoutContextCopy(uiLayout *layout, struct bContextStore *context);

/**
 * Set tooltip function for all buttons in the layout.
 * func, arg and free_arg are passed on to UI_but_func_tooltip_set, so their meaning is the same.
 *
 * \param func: The callback function that gets called to get tooltip content
 * \param arg: An optional opaque pointer that gets passed to func
 * \param free_arg: An optional callback for freeing arg (can be set to e.g. MEM_freeN)
 * \param copy_arg: An optional callback for duplicating arg in case UI_but_func_tooltip_set
 * is being called on multiple buttons (can be set to e.g. MEM_dupallocN). If set to NULL, arg will
 * be passed as-is to all buttons.
 */
void uiLayoutSetTooltipFunc(uiLayout *layout,
                            uiButToolTipFunc func,
                            void *arg,
                            uiCopyArgFunc copy_arg,
                            uiFreeArgFunc free_arg);

/**
 * This is a bit of a hack but best keep it in one place at least.
 */
struct wmOperatorType *UI_but_operatortype_get_from_enum_menu(struct uiBut *but,
                                                              struct PropertyRNA **r_prop);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
struct MenuType *UI_but_menutype_get(uiBut *but);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
struct PanelType *UI_but_paneltype_get(uiBut *but);
void UI_menutype_draw(struct bContext *C, struct MenuType *mt, struct uiLayout *layout);
/**
 * Used for popup panels only.
 */
void UI_paneltype_draw(struct bContext *C, struct PanelType *pt, struct uiLayout *layout);

/* Only for convenience. */
void uiLayoutSetContextFromBut(uiLayout *layout, uiBut *but);

void uiLayoutSetOperatorContext(uiLayout *layout, wmOperatorCallContext opcontext);
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

wmOperatorCallContext uiLayoutGetOperatorContext(uiLayout *layout);
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

/* Layout create functions. */

uiLayout *uiLayoutRow(uiLayout *layout, bool align);
/**
 * See #uiLayoutColumnWithHeading().
 */
uiLayout *uiLayoutRowWithHeading(uiLayout *layout, bool align, const char *heading);
uiLayout *uiLayoutColumn(uiLayout *layout, bool align);
/**
 * Variant of #uiLayoutColumn() that sets a heading label for the layout if the first item is
 * added through #uiItemFullR(). If split layout is used and the item has no string to add to the
 * first split-column, the heading is added there instead. Otherwise the heading inserted with a
 * new row.
 */
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
                  bool live_icon,
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
                         bool hide_buttons);
/**
 * Version of #uiTemplateID using tabs.
 */
void uiTemplateIDTabs(uiLayout *layout,
                      struct bContext *C,
                      struct PointerRNA *ptr,
                      const char *propname,
                      const char *newop,
                      const char *menu,
                      int filter);
/**
 * This is for selecting the type of ID-block to use,
 * and then from the relevant type choosing the block to use.
 *
 * \param propname: property identifier for property that ID-pointer gets stored to.
 * \param proptypename: property identifier for property
 * used to determine the type of ID-pointer that can be used.
 */
void uiTemplateAnyID(uiLayout *layout,
                     struct PointerRNA *ptr,
                     const char *propname,
                     const char *proptypename,
                     const char *text);
/**
 * Search menu to pick an item from a collection.
 * A version of uiTemplateID that works for non-ID types.
 */
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
                             int rows,
                             int cols);
/**
 * This is creating/editing RNA-Paths
 *
 * - ptr: struct which holds the path property
 * - propname: property identifier for property that path gets stored to
 * - root_ptr: struct that path gets built from
 */
void uiTemplatePathBuilder(uiLayout *layout,
                           struct PointerRNA *ptr,
                           const char *propname,
                           struct PointerRNA *root_ptr,
                           const char *text);
void uiTemplateModifiers(uiLayout *layout, struct bContext *C);
void uiTemplateGpencilModifiers(uiLayout *layout, struct bContext *C);
/**
 * Check if the shader effect panels don't match the data and rebuild the panels if so.
 */
void uiTemplateShaderFx(uiLayout *layout, struct bContext *C);
/**
 * Check if the constraint panels don't match the data and rebuild the panels if so.
 */
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
/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
void uiTemplateIcon(uiLayout *layout, int icon_value, float icon_scale);
/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
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
/**
 * Template for a path creation widget intended for custom bevel profiles.
 * This section is quite similar to #uiTemplateCurveMapping, but with reduced complexity.
 */
void uiTemplateCurveProfile(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
/**
 * This template now follows User Preference for type - name is not correct anymore.
 */
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
/**
 * \todo for now, grouping of layers is determined by dividing up the length of
 * the array of layer bitflags
 */
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

/**
 * Draw Operator property buttons for redoing execution with different settings.
 * This function does not initialize the layout,
 * functions can be called on the layout before and after.
 */
void uiTemplateOperatorPropertyButs(const struct bContext *C,
                                    uiLayout *layout,
                                    struct wmOperator *op,
                                    eButLabelAlign label_align,
                                    short flag);
void uiTemplateHeader3D_mode(uiLayout *layout, struct bContext *C);
void uiTemplateEditModeSelection(uiLayout *layout, struct bContext *C);
void uiTemplateReportsBanner(uiLayout *layout, struct bContext *C);
void uiTemplateInputStatus(uiLayout *layout, struct bContext *C);
void uiTemplateStatusInfo(uiLayout *layout, struct bContext *C);
void uiTemplateKeymapItemProperties(uiLayout *layout, struct PointerRNA *ptr);

bool uiTemplateEventFromKeymapItem(struct uiLayout *layout,
                                   const char *text,
                                   const struct wmKeyMapItem *kmi,
                                   bool text_fallback);

void uiTemplateComponentMenu(uiLayout *layout,
                             struct PointerRNA *ptr,
                             const char *propname,
                             const char *name);
void uiTemplateNodeSocket(uiLayout *layout, struct bContext *C, float color[4]);

/**
 * Draw the main CacheFile properties and operators (file path, scale, etc.), that is those which
 * do not have their own dedicated template functions.
 */
void uiTemplateCacheFile(uiLayout *layout,
                         const struct bContext *C,
                         struct PointerRNA *ptr,
                         const char *propname);

/**
 * Lookup the CacheFile PointerRNA of the given pointer and return it in the output parameter.
 * Returns true if `ptr` has a RNACacheFile, false otherwise. If false, the output parameter is not
 * initialized.
 */
bool uiTemplateCacheFilePointer(struct PointerRNA *ptr,
                                const char *propname,
                                struct PointerRNA *r_file_ptr);

/**
 * Draw the velocity related properties of the CacheFile.
 */
void uiTemplateCacheFileVelocity(uiLayout *layout, struct PointerRNA *fileptr);

/**
 * Draw the render procedural related properties of the CacheFile.
 */
void uiTemplateCacheFileProcedural(uiLayout *layout,
                                   const struct bContext *C,
                                   struct PointerRNA *fileptr);

/**
 * Draw the time related properties of the CacheFile.
 */
void uiTemplateCacheFileTimeSettings(uiLayout *layout, struct PointerRNA *fileptr);

/**
 * Draw the override layers related properties of the CacheFile.
 */
void uiTemplateCacheFileLayers(uiLayout *layout,
                               const struct bContext *C,
                               struct PointerRNA *fileptr);

/** Default UIList class name, keep in sync with its declaration in `bl_ui/__init__.py`. */
#define UI_UL_DEFAULT_CLASS_NAME "UI_UL_list"
enum uiTemplateListFlags {
  UI_TEMPLATE_LIST_FLAG_NONE = 0,
  UI_TEMPLATE_LIST_SORT_REVERSE = (1 << 0),
  UI_TEMPLATE_LIST_SORT_LOCK = (1 << 1),
  /** Don't allow resizing the list, i.e. don't add the grip button. */
  UI_TEMPLATE_LIST_NO_GRIP = (1 << 2),
  /** Do not show filtering options, not even the button to expand/collapse them. Also hides the
   * grip button. */
  UI_TEMPLATE_LIST_NO_FILTER_OPTIONS = (1 << 3),
  /** For #UILST_LAYOUT_BIG_PREVIEW_GRID, don't reserve space for the name label. */
  UI_TEMPLATE_LIST_NO_NAMES = (1 << 4),

  UI_TEMPLATE_LIST_FLAGS_LAST
};
ENUM_OPERATORS(uiTemplateListFlags, UI_TEMPLATE_LIST_FLAGS_LAST);

void uiTemplateList(uiLayout *layout,
                    const struct bContext *C,
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
                    enum uiTemplateListFlags flags);
struct uiList *uiTemplateList_ex(uiLayout *layout,
                                 const struct bContext *C,
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
                                 enum uiTemplateListFlags flags,
                                 void *customdata);

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
void uiTemplateNodeAssetMenuItems(uiLayout *layout, struct bContext *C, const char *catalog_path);
void uiTemplateTextureUser(uiLayout *layout, struct bContext *C);
/**
 * Button to quickly show texture in Properties Editor texture tab.
 */
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

enum {
  UI_TEMPLATE_ASSET_DRAW_NO_NAMES = (1 << 0),
  UI_TEMPLATE_ASSET_DRAW_NO_FILTER = (1 << 1),
  UI_TEMPLATE_ASSET_DRAW_NO_LIBRARY = (1 << 2),
};
void uiTemplateAssetView(struct uiLayout *layout,
                         const struct bContext *C,
                         const char *list_id,
                         struct PointerRNA *asset_library_dataptr,
                         const char *asset_library_propname,
                         struct PointerRNA *assets_dataptr,
                         const char *assets_propname,
                         struct PointerRNA *active_dataptr,
                         const char *active_propname,
                         const struct AssetFilterSettings *filter_settings,
                         int display_flags,
                         const char *activate_opname,
                         struct PointerRNA *r_activate_op_properties,
                         const char *drag_opname,
                         struct PointerRNA *r_drag_op_properties);

void uiTemplateLightLinkingCollection(struct uiLayout *layout,
                                      struct PointerRNA *ptr,
                                      const char *propname);

void uiTemplateGreasePencilLayerTree(uiLayout *layout, struct bContext *C);

/**
 * \return: A RNA pointer for the operator properties.
 */
struct PointerRNA *UI_list_custom_activate_operator_set(struct uiList *ui_list,
                                                        const char *opname,
                                                        bool create_properties);
/**
 * \return: A RNA pointer for the operator properties.
 */
struct PointerRNA *UI_list_custom_drag_operator_set(struct uiList *ui_list,
                                                    const char *opname,
                                                    bool create_properties);

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
/**
 * For use in cases where we have.
 */
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
                     wmOperatorCallContext context,
                     int flag,
                     struct PointerRNA *r_opptr);
void uiItemFullO(uiLayout *layout,
                 const char *opname,
                 const char *name,
                 int icon,
                 struct IDProperty *properties,
                 wmOperatorCallContext context,
                 int flag,
                 struct PointerRNA *r_opptr);
void uiItemFullOMenuHold_ptr(uiLayout *layout,
                             struct wmOperatorType *ot,
                             const char *name,
                             int icon,
                             struct IDProperty *properties,
                             wmOperatorCallContext context,
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
/**
 * Use a wrapper function since re-implementing all the logic in this function would be messy.
 */
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
                         int icon,
                         bool results_are_suggestions);
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
                      wmOperatorCallContext context,
                      int flag);
/**
 * Create UI items for enum items in \a item_array.
 *
 * A version of #uiItemsFullEnumO that takes pre-calculated item array.
 */
void uiItemsFullEnumO_items(uiLayout *layout,
                            struct wmOperatorType *ot,
                            struct PointerRNA ptr,
                            struct PropertyRNA *prop,
                            struct IDProperty *properties,
                            wmOperatorCallContext context,
                            int flag,
                            const struct EnumPropertyItem *item_array,
                            int totitem);

typedef struct uiPropertySplitWrapper {
  uiLayout *label_column;
  uiLayout *property_row;
  uiLayout *decorate_column;
} uiPropertySplitWrapper;

/**
 * Normally, we handle the split layout in #uiItemFullR(), but there are other cases where the
 * logic is needed. Ideally, #uiItemFullR() could just call this, but it currently has too many
 * special needs.
 */
uiPropertySplitWrapper uiItemPropertySplitWrapperCreate(uiLayout *parent_layout);

void uiItemL(uiLayout *layout, const char *name, int icon); /* label */
struct uiBut *uiItemL_ex(
    uiLayout *layout, const char *name, int icon, bool highlight, bool redalert);
/**
 * Helper to add a label and creates a property split layout if needed.
 */
uiLayout *uiItemL_respect_property_split(uiLayout *layout, const char *text, int icon);
/**
 * Label icon for dragging.
 */
void uiItemLDrag(uiLayout *layout, struct PointerRNA *ptr, const char *name, int icon);
/**
 * Menu.
 */
void uiItemM_ptr(uiLayout *layout, struct MenuType *mt, const char *name, int icon);
void uiItemM(uiLayout *layout, const char *menuname, const char *name, int icon);
/**
 * Menu contents.
 */
void uiItemMContents(uiLayout *layout, const char *menuname);

/* Decorators. */

/**
 * Insert a decorator item for a button with the same property as \a prop.
 * To force inserting a blank dummy element, NULL can be passed for \a ptr and \a prop.
 */
void uiItemDecoratorR_prop(uiLayout *layout,
                           struct PointerRNA *ptr,
                           struct PropertyRNA *prop,
                           int index);
/**
 * Insert a decorator item for a button with the same property as \a prop.
 * To force inserting a blank dummy element, NULL can be passed for \a ptr and \a propname.
 */
void uiItemDecoratorR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int index);
/** Value item */
void uiItemV(uiLayout *layout, const char *name, int icon, int argval);
/** Separator item */
void uiItemS(uiLayout *layout);
/** Separator item */
void uiItemS_ex(uiLayout *layout, float factor);
/** Flexible spacing. */
void uiItemSpacer(uiLayout *layout);

void uiItemProgressIndicator(uiLayout *layout,
                             const char *text,
                             float factor,
                             enum eButProgressType progress_type);

/* popover */
void uiItemPopoverPanel_ptr(
    uiLayout *layout, const struct bContext *C, struct PanelType *pt, const char *name, int icon);
void uiItemPopoverPanel(uiLayout *layout,
                        const struct bContext *C,
                        const char *panel_type,
                        const char *name,
                        int icon);
void uiItemPopoverPanelFromGroup(uiLayout *layout,
                                 struct bContext *C,
                                 int space_id,
                                 int region_id,
                                 const char *context,
                                 const char *category);

/**
 * Level items.
 */
void uiItemMenuF(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *arg);
/**
 * Version of #uiItemMenuF that free's `argN`.
 */
void uiItemMenuFN(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *argN);
void uiItemMenuEnumFullO_ptr(uiLayout *layout,
                             const struct bContext *C,
                             struct wmOperatorType *ot,
                             const char *propname,
                             const char *name,
                             int icon,
                             struct PointerRNA *r_opptr);
void uiItemMenuEnumFullO(uiLayout *layout,
                         const struct bContext *C,
                         const char *opname,
                         const char *propname,
                         const char *name,
                         int icon,
                         struct PointerRNA *r_opptr);
void uiItemMenuEnumO(uiLayout *layout,
                     const struct bContext *C,
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
/**
 * Evaluate layout items as a Python dictionary.
 */
const char *UI_layout_introspect(uiLayout *layout);

/**
 * Helper to add a big icon and create a split layout for alert popups.
 * Returns the layout to place further items into the alert box.
 */
uiLayout *uiItemsAlertBox(uiBlock *block, int size, eAlertIcon icon);

/* UI Operators */
typedef struct uiDragColorHandle {
  float color[3];
  bool gamma_corrected;
} uiDragColorHandle;

void ED_operatortypes_ui(void);
/**
 * \brief User Interface Keymap
 */
void ED_keymap_ui(struct wmKeyConfig *keyconf);
void ED_dropboxes_ui(void);
void ED_uilisttypes_ui(void);

void UI_drop_color_copy(struct bContext *C, struct wmDrag *drag, struct wmDropBox *drop);
bool UI_drop_color_poll(struct bContext *C, struct wmDrag *drag, const struct wmEvent *event);

bool UI_context_copy_to_selected_list(struct bContext *C,
                                      struct PointerRNA *ptr,
                                      struct PropertyRNA *prop,
                                      struct ListBase *r_lb,
                                      bool *r_use_path_from_id,
                                      char **r_path);
bool UI_context_copy_to_selected_check(struct PointerRNA *ptr,
                                       struct PointerRNA *ptr_link,
                                       struct PropertyRNA *prop,
                                       const char *path,
                                       bool use_path_from_id,
                                       struct PointerRNA *r_ptr,
                                       struct PropertyRNA **r_prop);

/* Helpers for Operators */
uiBut *UI_context_active_but_get(const struct bContext *C);
/**
 * Version of #UI_context_active_get() that uses the result of #CTX_wm_menu()
 * if set. Does not traverse into parent menus, which may be wanted in some
 * cases.
 */
uiBut *UI_context_active_but_get_respect_menu(const struct bContext *C);
/**
 * Version of #UI_context_active_but_get that also returns RNA property info.
 * Helper function for insert keyframe, reset to default, etc operators.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
uiBut *UI_context_active_but_prop_get(const struct bContext *C,
                                      struct PointerRNA *r_ptr,
                                      struct PropertyRNA **r_prop,
                                      int *r_index);

/**
 * As above, but for a specified region.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
uiBut *UI_region_active_but_prop_get(const struct ARegion *region,
                                     struct PointerRNA *r_ptr,
                                     struct PropertyRNA **r_prop,
                                     int *r_index);

void UI_context_active_but_prop_handle(struct bContext *C, bool handle_undo);
void UI_context_active_but_clear(struct bContext *C, struct wmWindow *win, struct ARegion *region);

struct wmOperator *UI_context_active_operator_get(const struct bContext *C);
/**
 * Helper function for insert keyframe, reset to default, etc operators.
 */
void UI_context_update_anim_flag(const struct bContext *C);
void UI_context_active_but_prop_get_filebrowser(const struct bContext *C,
                                                struct PointerRNA *r_ptr,
                                                struct PropertyRNA **r_prop,
                                                bool *r_is_undo,
                                                bool *r_is_userdef);
/**
 * For new/open operators.
 *
 * This is for browsing and editing the ID-blocks used.
 */
void UI_context_active_but_prop_get_templateID(struct bContext *C,
                                               struct PointerRNA *r_ptr,
                                               struct PropertyRNA **r_prop);
struct ID *UI_context_active_but_get_tab_ID(struct bContext *C);

uiBut *UI_region_active_but_get(const struct ARegion *region);
uiBut *UI_region_but_find_rect_over(const struct ARegion *region, const struct rcti *rect_px);
uiBlock *UI_region_block_find_mouse_over(const struct ARegion *region,
                                         const int xy[2],
                                         bool only_clip);
/**
 * Try to find a search-box region opened from a button in \a button_region.
 */
struct ARegion *UI_region_searchbox_region_get(const struct ARegion *button_region);

/** #uiFontStyle.align */
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
                          size_t str_len,
                          const uchar col[4],
                          const struct uiFontStyleDraw_Params *fs_params,
                          int *r_xofs,
                          int *r_yofs,
                          struct ResultBLF *r_info);

void UI_fontstyle_draw(const struct uiFontStyle *fs,
                       const struct rcti *rect,
                       const char *str,
                       size_t str_len,
                       const uchar col[4],
                       const struct uiFontStyleDraw_Params *fs_params);
/**
 * Drawn same as above, but at 90 degree angle.
 */
void UI_fontstyle_draw_rotated(const struct uiFontStyle *fs,
                               const struct rcti *rect,
                               const char *str,
                               const uchar col[4]);
/**
 * Similar to #UI_fontstyle_draw
 * but ignore alignment, shadow & no clipping rect.
 *
 * For drawing on-screen labels.
 */
void UI_fontstyle_draw_simple(
    const struct uiFontStyle *fs, float x, float y, const char *str, const uchar col[4]);
/**
 * Same as #UI_fontstyle_draw but draw a colored backdrop.
 */
void UI_fontstyle_draw_simple_backdrop(const struct uiFontStyle *fs,
                                       float x,
                                       float y,
                                       const char *str,
                                       const float col_fg[4],
                                       const float col_bg[4]);

int UI_fontstyle_string_width(const struct uiFontStyle *fs,
                              const char *str) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
/**
 * Return the width of `str` with the spacing & kerning of `fs` with `aspect`
 * (representing #uiBlock.aspect) applied.
 *
 * When calculating text width, the UI layout logic calculate widths without scale,
 * only applying scale when drawing. This causes problems for fonts since kerning at
 * smaller sizes often makes them wider than a scaled down version of the larger text.
 * Resolve this by calculating the text at the on-screen size,
 * returning the result scaled back to 1:1. See #92361.
 */
int UI_fontstyle_string_width_with_block_aspect(const struct uiFontStyle *fs,
                                                const char *str,
                                                float aspect) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
int UI_fontstyle_height_max(const struct uiFontStyle *fs);

/**
 * Triangle 'icon' for panel header and other cases.
 */
void UI_draw_icon_tri(float x, float y, char dir, const float[4]);

/**
 * Read a style (without any scaling applied).
 */
const struct uiStyle *UI_style_get(void); /* use for fonts etc */
/**
 * Read a style (with the current DPI applied).
 */
const struct uiStyle *UI_style_get_dpi(void);

/* linker workaround ack! */
void UI_template_fix_linking(void);

/* UI_OT_editsource helpers */
bool UI_editsource_enable_check(void);
void UI_editsource_active_but_test(uiBut *but);
/**
 * Remove the editsource data for \a old_but and reinsert it for \a new_but. Use when the button
 * was reallocated, e.g. to have a new type (#ui_but_change_type()).
 */
void UI_editsource_but_replace(const uiBut *old_but, uiBut *new_but);

/**
 * Adjust the view so the rectangle of \a but is in view, with some extra margin.
 *
 * It's important that this is only executed after buttons received their final #uiBut.rect. E.g.
 * #UI_panels_end() modifies them, so if that is executed, this function must not be called before
 * it.
 *
 * \param region: The region the button is placed in. Make sure this is actually the one the button
 *                is placed in, not just the context region.
 */
void UI_but_ensure_in_view(const struct bContext *C, struct ARegion *region, const uiBut *but);

/* UI_butstore_ helpers */
typedef struct uiButStore uiButStore;
typedef struct uiButStoreElem uiButStoreElem;

/**
 * Create a new button store, the caller must manage and run #UI_butstore_free
 */
uiButStore *UI_butstore_create(uiBlock *block);
/**
 * NULL all pointers, don't free since the owner needs to be able to inspect.
 */
void UI_butstore_clear(uiBlock *block);
/**
 * Map freed buttons from the old block and update pointers.
 */
void UI_butstore_update(uiBlock *block);
void UI_butstore_free(uiBlock *block, uiButStore *bs);
bool UI_butstore_is_valid(uiButStore *bs);
bool UI_butstore_is_registered(uiBlock *block, uiBut *but);
void UI_butstore_register(uiButStore *bs_handle, uiBut **but_p);
/**
 * Update the pointer for a registered button.
 */
bool UI_butstore_register_update(uiBlock *block, uiBut *but_dst, const uiBut *but_src);
void UI_butstore_unregister(uiButStore *bs_handle, uiBut **but_p);

/**
 * A version of #WM_key_event_operator_string that's limited to UI elements.
 *
 * This supports showing shortcuts in context-menus (for example),
 * for actions that can also be activated using shortcuts while the cursor is over the button.
 * Without this those shortcuts aren't discoverable for users.
 */
const char *UI_key_event_operator_string(const struct bContext *C,
                                         const char *opname,
                                         IDProperty *properties,
                                         const bool is_strict,
                                         char *result,
                                         const int result_maxncpy);

/* ui_interface_region_tooltip.c */

/**
 * \param is_label: When true, show a small tip that only shows the name, otherwise show the full
 *                  tooltip.
 */
struct ARegion *UI_tooltip_create_from_button(struct bContext *C,
                                              struct ARegion *butregion,
                                              uiBut *but,
                                              bool is_label);
struct ARegion *UI_tooltip_create_from_button_or_extra_icon(struct bContext *C,
                                                            struct ARegion *butregion,
                                                            uiBut *but,
                                                            uiButExtraOpIcon *extra_icon,
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

/**
 * Create a tooltip from search-item tooltip data \a item_tooltip data.
 * To be called from a callback set with #UI_but_func_search_set_tooltip().
 *
 * \param item_rect: Rectangle of the search item in search region space (#ui_searchbox_butrect())
 *                   which is passed to the tooltip callback.
 */
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

/**
 * Returns the best "UI" precision for given floating value,
 * so that e.g. 10.000001 rather gets drawn as '10'...
 */
int UI_calc_float_precision(int prec, double value);

/* widget batched drawing */
void UI_widgetbase_draw_cache_begin(void);
void UI_widgetbase_draw_cache_flush(void);
void UI_widgetbase_draw_cache_end(void);

/* Use for resetting the theme. */
/**
 * Initialize default theme.
 *
 * \note When you add new colors, created & saved themes need initialized
 * use function below, #init_userdef_do_versions.
 */
void UI_theme_init_default(void);
void UI_style_init_default(void);

void UI_interface_tag_script_reload(void);

/** Special drawing for toolbar, mainly workarounds for inflexible icon sizing. */
#define USE_UI_TOOLBAR_HACK

/** Support click-drag motion which presses the button and closes a popover (like a menu). */
#define USE_UI_POPOVER_ONCE

/**
 * Call the #ui::AbstractView::begin_filtering() function of the view to enable filtering.
 * Typically used to enable a filter text button. Triggered on Ctrl+F by default.
 * \return True when filtering was enabled successfully.
 */
bool UI_view_begin_filtering(const struct bContext *C, const uiViewHandle *view_handle);

bool UI_view_item_is_interactive(const uiViewItemHandle *item_handle);
bool UI_view_item_is_active(const uiViewItemHandle *item_handle);
bool UI_view_item_matches(const uiViewItemHandle *a_handle, const uiViewItemHandle *b_handle);
/**
 * Can \a item_handle be renamed right now? Note that this isn't just a mere wrapper around
 * #AbstractViewItem::supports_renaming(). This also checks if there is another item being renamed,
 * and returns false if so.
 */
bool UI_view_item_can_rename(const uiViewItemHandle *item_handle);
void UI_view_item_begin_rename(uiViewItemHandle *item_handle);

void UI_view_item_context_menu_build(struct bContext *C,
                                     const uiViewItemHandle *item_handle,
                                     uiLayout *column);

bool UI_view_item_supports_drag(const uiViewItemHandle *item_);
/**
 * Attempt to start dragging \a item_. This will not work if the view item doesn't
 * support dragging, i.e. if it won't create a drag-controller upon request.
 * \return True if dragging started successfully, otherwise false.
 */
bool UI_view_item_drag_start(struct bContext *C, const uiViewItemHandle *item_);

/**
 * \param xy: Coordinate to find a view item at, in window space.
 * \param pad: Extra padding added to the bounding box of the view.
 */
uiViewHandle *UI_region_view_find_at(const struct ARegion *region, const int xy[2], int pad);
/**
 * \param xy: Coordinate to find a view item at, in window space.
 */
uiViewItemHandle *UI_region_views_find_item_at(const struct ARegion *region, const int xy[2])
    ATTR_NONNULL();
uiViewItemHandle *UI_region_views_find_active_item(const struct ARegion *region);
uiBut *UI_region_views_find_active_item_but(const struct ARegion *region);

#ifdef __cplusplus
}
#endif
