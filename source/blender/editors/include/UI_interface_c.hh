/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#include <functional>
#include <optional>
#include <string>

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8_symbols.h"
#include "BLI_sys_types.h" /* size_t */

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"

#include "UI_interface_icons.hh"
#include "UI_interface_types.hh"

#include "WM_types.hh"

#include "MEM_guardedalloc.h"

namespace blender {

/* Struct Declarations */

struct ARegion;
struct Depsgraph;
struct EnumPropertyItem;
struct FileSelectParams;
struct ID;
struct IDProperty;
struct ImBuf;
struct Image;
struct ImageUser;
struct MTex;
struct Panel;
struct PanelType;
struct PanelCategoryDyn;
struct PanelCategoryStack;
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
struct MenuType;
struct rctf;
struct rcti;
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
namespace ed::asset {
struct AssetFilterSettings;
}
namespace ui {
class AbstractView;
class AbstractViewItem;
struct Layout;
struct Button;
struct ButtonSearch;
struct ButtonExtraOpIcon;
struct TooltipData;
struct PopupBlockHandle;
struct Block;
}  // namespace ui

/* Defines */

namespace ui {

/**
 * Character used for splitting labels (right align text after this character).
 * Users should never see this character.
 * Only applied when #BUT_HAS_SEP_CHAR flag is enabled, see it's doc-string for details.
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

/**
 * For #ARegion.overlap regions, pass events though if they don't overlap
 * the regions contents (the usable part of the #View2D and buttons).
 *
 * The margin is needed so it's not possible to accidentally click in between buttons.
 */
#define UI_REGION_OVERLAP_MARGIN (U.widget_unit / 3)

/** Use for clamping popups within the screen. */
#define UI_SCREEN_MARGIN 10

/** #Block.emboss and #Button.emboss */
enum class EmbossType : uint8_t {
  /** Use widget style for drawing. */
  Emboss = 0,
  /** Nothing, only icon and/or text */
  None = 1,
  /** Pull-down menu style */
  Pulldown = 2,
  /** Pie Menu */
  PieMenu = 3,
  /**
   * The same as #EmbossType::None, unless the button has
   * a coloring status like an animation state or red alert.
   */
  NoneOrStatus = 4,
  /** For layout engine, use emboss from block. */
  Undefined = 255,
};

/** #Block::direction */
enum {
  UI_DIR_UP = 1 << 0,
  UI_DIR_DOWN = 1 << 1,
  UI_DIR_LEFT = 1 << 2,
  UI_DIR_RIGHT = 1 << 3,
  UI_DIR_CENTER_X = 1 << 4,
  UI_DIR_CENTER_Y = 1 << 5,

  UI_DIR_ALL = UI_DIR_UP | UI_DIR_DOWN | UI_DIR_LEFT | UI_DIR_RIGHT,
};

/** #Block.flag (controls) */
enum {
  BLOCK_LOOP = 1 << 0,
  BLOCK_NUMSELECT = 1 << 1,
  /** Don't apply window clipping. */
  BLOCK_NO_WIN_CLIP = 1 << 2,
  BLOCK_CLIPBOTTOM = 1 << 3,
  BLOCK_CLIPTOP = 1 << 4,
  BLOCK_MOVEMOUSE_QUIT = 1 << 5,
  BLOCK_KEEP_OPEN = 1 << 6,
  BLOCK_POPUP = 1 << 7,
  BLOCK_OUT_1 = 1 << 8,
  BLOCK_SEARCH_MENU = 1 << 9,
  BLOCK_POPUP_MEMORY = 1 << 10,
  /** Stop handling mouse events. */
  BLOCK_CLIP_EVENTS = 1 << 11,

  /* #Block::flags bits 14-17 are identical to #Button::drawflag bits. */

  BLOCK_POPUP_HOLD = 1 << 18,
  BLOCK_LIST_ITEM = 1 << 19,
  BLOCK_PIE_MENU = 1 << 20,
  BLOCK_POPOVER = 1 << 21,
  BLOCK_POPOVER_ONCE = 1 << 22,
  /** Always show key-maps, even for non-menus. */
  BLOCK_SHOW_SHORTCUT_ALWAYS = 1 << 23,
  /** Don't show library override state for buttons in this block. */
  BLOCK_NO_DRAW_OVERRIDDEN_STATE = 1 << 24,
  /** The block is only used during the search process and will not be drawn.
   * Currently just for the case of a closed panel's sub-panel (and its sub-panels). */
  BLOCK_SEARCH_ONLY = 1 << 25,
  /** Hack for quick setup (splash screen) to draw text centered. */
  BLOCK_QUICK_SETUP = 1 << 26,
  /** Don't accelerator keys for the items in the block. */
  BLOCK_NO_ACCELERATOR_KEYS = 1 << 27,
};

/** #PopupBlockHandle.menuretval */
enum {
  /** Cancel all menus cascading. */
  RETURN_CANCEL = 1 << 0,
  /** Choice made. */
  RETURN_OK = 1 << 1,
  /** Left the menu. */
  RETURN_OUT = 1 << 2,
  /** Let the parent handle this event. */
  RETURN_OUT_PARENT = 1 << 3,
  /** Update the button that opened. */
  RETURN_UPDATE = 1 << 4,
  /** Popup is ok to be handled. */
  RETURN_POPUP_OK = 1 << 5,
};

/** #Button.flag general state flags. */
enum ButtonFlag {
  /* WARNING: the first 8 flags are internal (see #UI_SELECT definition). */

  BUT_ICON_SUBMENU = 1 << 8,
  BUT_ICON_PREVIEW = 1 << 9,

  BUT_NODE_LINK = 1 << 10,
  BUT_NODE_ACTIVE = 1 << 11,
  BUT_DRAG_LOCK = 1 << 12,
  /** Grayed out and un-editable. */
  BUT_DISABLED = 1 << 13,

  BUT_ANIMATED = 1 << 14,
  BUT_ANIMATED_KEY = 1 << 15,
  BUT_DRIVEN = 1 << 16,
  BUT_REDALERT = 1 << 17,
  /** Grayed out but still editable. */
  BUT_INACTIVE = 1 << 18,
  BUT_LAST_ACTIVE = 1 << 19,
  BUT_UNDO = 1 << 20,
  /* UNUSED = 1 << 21, */
  BUT_NO_UTF8 = 1 << 22,

  /** For popups, pressing return activates this button, overriding the highlighted button.
   * For non-popups this is just used as a display hint for the user to let them
   * know the action which is activated when pressing return (file selector for eg). */
  BUT_ACTIVE_DEFAULT = 1 << 23,

  /** This but is "inside" a list item (currently used to change theme colors). */
  BUT_LIST_ITEM = 1 << 24,
  /** edit this button as well as the active button (not just dragging) */
  BUT_DRAG_MULTI = 1 << 25,
  /** Use for popups to start editing the button on initialization. */
  BUT_ACTIVATE_ON_INIT = 1 << 26,

  /**
   * #Button.str contains #UI_SEP_CHAR, used to show key-shortcuts right aligned.
   *
   * Since a label may contain #UI_SEP_CHAR, it's important to split on the last occurrence
   * (meaning the right aligned text can't contain this character).
   */
  BUT_HAS_SEP_CHAR = 1 << 27,
  /** Don't run updates while dragging (needed in rare cases). */
  BUT_UPDATE_DELAY = 1 << 28,
  /** When widget is in text-edit mode, update value on each char stroke. */
  BUT_TEXTEDIT_UPDATE = 1 << 29,
  /** Show 'x' icon to clear/unlink value of text or search button. */
  BUT_VALUE_CLEAR = 1 << 30,

  /** RNA property of the button is overridden from linked reference data. */
  BUT_OVERRIDDEN = 1u << 31u,
};

enum {
  /**
   * This is used when `BUT_ACTIVATE_ON_INIT` is used, which is used to activate e.g. a search
   * box as soon as a popup opens. Usually, the text in the search box is selected by default.
   * However, sometimes this behavior is not desired, so it can be disabled with this flag.
   */
  BUT2_ACTIVATE_ON_INIT_NO_SELECT = 1 << 0,
  /**
   * Force the button as active in a semi-modal state. For example, text buttons can continuously
   * capture text input, while leaving the remaining UI interactive. Only supported well for text
   * buttons currently.
   */
  BUT2_FORCE_SEMI_MODAL_ACTIVE = 1 << 1,
};

/** #Button.dragflag */
enum {
  /** By default only the left part of a button triggers dragging. A questionable design to make
   * the icon but not other parts of the button draggable. Set this flag so the entire button can
   * be dragged. */
  BUT_DRAG_FULL_BUT = (1 << 0),

  /* --- Internal flags. --- */
  BUT_DRAGPOIN_FREE = (1 << 1),
};

/** Default font size for normal text. */
#define UI_DEFAULT_TEXT_POINTS 11.0f

/** Larger size used for title text. */
#define UI_DEFAULT_TITLE_POINTS 11.0f

/** Size of tooltip text. */
#define UI_DEFAULT_TOOLTIP_POINTS 11.0f

#define UI_PANEL_WIDTH 340
#define UI_COMPACT_PANEL_WIDTH 160
#define UI_SIDEBAR_PANEL_WIDTH 280
#define UI_NAVIGATION_REGION_WIDTH UI_COMPACT_PANEL_WIDTH
#define UI_NARROW_NAVIGATION_REGION_WIDTH 100

/** The width of one icon column of the Toolbar. */
#define UI_TOOLBAR_COLUMN (1.25f * ICON_DEFAULT_HEIGHT_TOOLBAR)
/** The space between the Toolbar and the area's edge. */
#define UI_TOOLBAR_MARGIN (0.5f * ICON_DEFAULT_HEIGHT_TOOLBAR)
/** Total width of Toolbar showing one icon column. */
#define UI_TOOLBAR_WIDTH UI_TOOLBAR_MARGIN + UI_TOOLBAR_COLUMN

#define UI_PANEL_CATEGORY_MARGIN_WIDTH (U.widget_unit * 1.0f)

/* Minimum width for a panel showing only category tabs. */
#define UI_PANEL_CATEGORY_MIN_WIDTH 26.0f
/* Minimum width for a panel showing content and category tabs. */
#define UI_PANEL_CATEGORY_MIN_SNAP_WIDTH 90.0f

/* Both these margins should be ignored if the panel doesn't show a background (check
 * #panel_should_show_background()). */
#define UI_PANEL_MARGIN_X (U.widget_unit * 0.4f)
#define UI_PANEL_MARGIN_Y (U.widget_unit * 0.1f)

/**
 * #Button::drawflag, these flags should only affect how the button is drawn.
 *
 * \note currently, these flags *are not passed* to the widgets state() or draw() functions
 * (except for the 'align' ones)!
 */
enum {
  /** Text and icon alignment (by default, they are centered). */
  BUT_TEXT_LEFT = 1 << 1,
  BUT_ICON_LEFT = 1 << 2,
  BUT_TEXT_RIGHT = 1 << 3,
  /** Prevent the button to show any tool-tip. */
  BUT_NO_TOOLTIP = 1 << 4,
  /**
   * See #button_func_quick_tooltip_set.
   */
  BUT_HAS_QUICK_TOOLTIP = 1 << 5,
  /** Do not add the usual horizontal padding for text drawing. */
  BUT_NO_TEXT_PADDING = 1 << 6,
  /** Do not add the usual padding around preview image drawing, use the size of the button. */
  BUT_NO_PREVIEW_PADDING = 1 << 7,

  /* Button align flag, for drawing groups together.
   * Used in 'Block.flag', take care! */
  BUT_ALIGN_TOP = 1 << 14,
  BUT_ALIGN_LEFT = 1 << 15,
  BUT_ALIGN_RIGHT = 1 << 16,
  BUT_ALIGN_DOWN = 1 << 17,
  BUT_ALIGN = BUT_ALIGN_TOP | BUT_ALIGN_LEFT | BUT_ALIGN_RIGHT | BUT_ALIGN_DOWN,
  /* end bits shared with 'Block.flag' */

  /**
   * Warning - HACK!
   * Needed for buttons which are not TOP/LEFT aligned,
   * but have some top/left corner stitched to some other TOP/LEFT-aligned button,
   * because of "corrective" hack in #widget_roundbox_set().
   */
  BUT_ALIGN_STITCH_TOP = 1 << 18,
  BUT_ALIGN_STITCH_LEFT = 1 << 19,
  BUT_ALIGN_ALL = BUT_ALIGN | BUT_ALIGN_STITCH_TOP | BUT_ALIGN_STITCH_LEFT,

  /** This but is "inside" a box item (currently used to change theme colors). */
  BUT_BOX_ITEM = 1 << 20,

  /** Mouse is hovering left part of number button */
  BUT_HOVER_LEFT = 1 << 21,
  /** Mouse is hovering right part of number button */
  BUT_HOVER_RIGHT = 1 << 22,

  /** Reverse order of consecutive off/on icons */
  BUT_ICON_REVERSE = 1 << 23,

  /** Value is animated, but the current value differs from the animated one. */
  BUT_ANIMATED_CHANGED = 1 << 24,

  /** Draw the checkbox buttons inverted. */
  BUT_CHECKBOX_INVERT = 1 << 25,

  /** Drawn in a way that indicates that the state/value is unknown. */
  BUT_INDETERMINATE = 1 << 26,

  /** Draw icon inverted to indicate a special state. */
  BUT_ICON_INVERT = 1 << 27,
};

enum class ButPointerType : uint8_t {
  None = 0,
  Char,
  Short,
  Int,
  Float,
  // ButPointerType::Function = 192, /* UNUSED */
  Bit = 1 << 7, /* OR'd with a bit index. */
};
ENUM_OPERATORS(ButPointerType);
/** \note requires `Button::poin != nullptr`. */
#define BUT_POIN_TYPES (ButPointerType::Float | ButPointerType::Short | ButPointerType::Char)

enum class ButtonType : int8_t {
  But = 1,
  Row,
  Text,
  /** Drop-down list. */
  Menu,
  ButMenu,
  /** Number button. */
  Num,
  /** Number slider. */
  NumSlider,
  Toggle,
  ToggleN,
  IconToggle,
  IconToggleN,
  /** Same as regular toggle, but no on/off state displayed. */
  ButToggle,
  /** Similar to toggle, display a 'tick'. */
  Checkbox,
  CheckboxN,
  Color,
  Tab,
  Popover,
  Scroll,
  Block,
  Label,
  KeyEvent,
  HsvCube,
  /** Menu (often used in headers), `*_MENU` with different draw-type. */
  Pulldown,
  Roundbox,
  ColorBand,
  /** Sphere widget (used to input a unit-vector, aka normal). */
  Unitvec,
  Curve,
  /** Profile editing widget. */
  CurveProfile,
  ListBox,
  ListRow,
  HsvCircle,
  TrackPreview,

  /** Buttons with value >= #ButtonType::SearchMenu don't get undo pushes. */
  SearchMenu,
  Extra,
  /** A preview image (#PreviewImage), with text under it. Typically bigger than normal buttons and
   * laid out in a grid, e.g. like the File Browser in thumbnail display mode. */
  PreviewTile,
  HotkeyEvent,
  /** Non-interactive image, used for splash screen. */
  Image,
  Histogram,
  Waveform,
  Vectorscope,
  Progress,
  NodeSocket,
  Sepr,
  SeprLine,
  /** Dynamically fill available space. */
  SeprSpacer,
  /** Resize handle (resize UI-list). */
  Grip,
  Decorator,
  /** An item a view (see #ui::AbstractViewItem). */
  ViewItem,
};

inline char but_pointer_bit_max_index(ButPointerType pointer_type)
{
  switch (pointer_type) {
    case ButPointerType::Char:
      return sizeof(char) * 8;
    case ButPointerType::Short:
      return sizeof(short) * 8;
    case ButPointerType::Int:
      return sizeof(int) * 8;
    default:
      break;
  }
  return 0;
}

struct ButtonTypeWithPointerType {
  ButtonType but_type = ButtonType::But;
  /**
   * Buttons can access source data with RNA pointers or raw pointers (#Button::poin), when using a
   * raw pointer to numerical values this indicates the underlying type of the source data.
   */
  ButPointerType pointer_type = ButPointerType::None;
  /**
   * Indicates the bit index when the raw pointed data stores boolean bit values,
   * which is indicated with the #ButPointerType::Bit flag.
   */
  char bit_index = 0;

  ButtonTypeWithPointerType(ButtonType bt) : but_type{bt} {}

  ButtonTypeWithPointerType(ButtonType bt, ButPointerType pt) : but_type{bt}, pointer_type{pt} {}

  ButtonTypeWithPointerType(ButtonType bt, ButPointerType pt, int i)
      : but_type{bt}, pointer_type{pt}, bit_index{char(i)}
  {
    BLI_assert(bool(pointer_type & ButPointerType::Bit));
    BLI_assert(bit_index >= 0);
    BLI_assert(bit_index < but_pointer_bit_max_index(pointer_type & ~ButPointerType::Bit));
  }
};

/** Gradient types, for color picker #ButtonType::HsvCube etc. */
enum eButGradientType {
  GRAD_NONE = -1,
  GRAD_SV = 0,
  GRAD_HV = 1,
  GRAD_HS = 2,
  GRAD_H = 3,
  GRAD_S = 4,
  GRAD_V = 5,

  GRAD_V_ALT = 9,
  GRAD_L_ALT = 10,
};

/* Drawing
 *
 * Functions to draw various shapes, taking theme settings into account.
 * Used for code that draws its own UI style elements. */

void draw_roundbox_corner_set(int type);
void draw_roundbox_aa(const rctf *rect, bool filled, float rad, const float color[4]);
void draw_roundbox_4fv(const rctf *rect, bool filled, float rad, const float col[4]);
void draw_roundbox_3ub_alpha(
    const rctf *rect, bool filled, float rad, const unsigned char col[3], unsigned char alpha);
void draw_roundbox_3fv_alpha(
    const rctf *rect, bool filled, float rad, const float col[3], float alpha);
void draw_roundbox_4fv_ex(const rctf *rect,
                          const float inner1[4],
                          const float inner2[4],
                          float shade_dir,
                          const float outline[4],
                          float outline_width,
                          float rad);

#if 0 /* unused */
int draw_roundbox_corner_get();
#endif

void draw_dropshadow(const rctf *rct, float radius, float width, float aspect, float alpha);

void draw_text_underline(int pos_x, int pos_y, int len, int height, const float color[4]);

/**
 * Draw title and text safe areas.
 *
 * \note This function is to be used with the 2D dashed shader enabled.
 *
 * \param pos: is a #PRIM_FLOAT, 2, #GPU_FETCH_FLOAT vertex attribute.
 * \param rect: The offsets for the view, not the zones.
 */
void draw_safe_areas(uint pos,
                     const rctf *rect,
                     const float title_aspect[2],
                     const float action_aspect[2]);

/** State for scroll-drawing. */
enum {
  SCROLL_PRESSED = 1 << 0,
  SCROLL_ARROWS = 1 << 1,
};
/**
 * Function in use for buttons and for view2d sliders.
 */
void draw_widget_scroll(uiWidgetColors *wcol, const rcti *rect, const rcti *slider, int state);

/**
 * Shortening string helper.
 *
 * Cut off the middle of the text to fit into the given width.
 *
 * If `rpart_sep` is not null, the part of `str` starting to first occurrence of `rpart_sep`
 * is preserved at all cost.
 * Useful for strings with shortcuts
 * (like `A Very Long Foo Bar Label For Menu Entry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O`).
 *
 * \param clip_right_if_tight: In case this middle clipping would just remove a few chars, or there
 * are less than 10 characters before the clipping, it rather clips right, which is more readable.
 */
float text_clip_middle_ex(const uiFontStyle *fstyle,
                          char *str,
                          float okwidth,
                          float minwidth,
                          size_t max_len,
                          char rpart_sep,
                          bool clip_right_if_tight = true);

Vector<StringRef> text_clip_multiline_middle(const uiFontStyle *fstyle,
                                             const char *str,
                                             char *clipped_str_buf,
                                             const size_t max_len_clipped_str_buf,
                                             const float max_line_width,
                                             const int max_lines);

/**
 * Callbacks.
 *
 * #block_func_handle_set/ButmFunc are for handling events through a callback.
 * HandleFunc gets the retval passed on, and ButmFunc gets a2. The latter is
 * mostly for compatibility with older code.
 *
 * - #button_func_complete_set is for tab completion.
 *
 * - #ButtonSearchFunc is for name buttons, showing a popup with matches
 *
 * - #block_func_set and button_func_set are callbacks run when a button is used,
 *   in case events, operators or RNA are not sufficient to handle the button.
 *
 * - #button_funcN_set will free the argument with MEM_freeN. */

struct SearchItems;

using ButtonHandleFunc = void (*)(bContext *C, void *arg1, void *arg2);
using ButtonHandleRenameFunc = void (*)(bContext *C, void *arg, char *origstr);
using ButtonHandleNFunc = void (*)(bContext *C, void *argN, void *arg2);
using ButtonHandleHoldFunc = void (*)(bContext *C, ARegion *butregion, Button *but);
using ButtonCompleteFunc = int (*)(bContext *C, char *str, void *arg);

/**
 * Signatures of callbacks used to free or copy some 'owned' void pointer data (like e.g.
 * #func_argN in #Button or #Block).
 */
using ButtonArgNFree = void (*)(void *argN);
using ButtonArgNCopy = void *(*)(const void *argN);

/**
 * Function to compare the identity of two buttons over redraws, to check if they represent the
 * same data, and thus should be considered the same button over redraws.
 */
using ButtonIdentityCompareFunc = bool (*)(const Button *a, const Button *b);

/* Search types. */
using ButtonSearchCreateFn = ARegion *(*)(bContext * C,
                                          ARegion *butregion,
                                          ButtonSearch *search_but);
/**
 * `is_first` is typically used to ignore search filtering when the menu is first opened in order
 * to display the full list of options. The value will be false after the button's text is edited
 * (for every call except the first).
 */
using ButtonSearchUpdateFn =
    void (*)(const bContext *C, void *arg, const char *str, SearchItems *items, bool is_first);
using ButtonSearchContextMenuFn = bool (*)(bContext *C,
                                           void *arg,
                                           void *active,
                                           const wmEvent *event);
using ButtonSearchTooltipFn =
    ARegion *(*)(bContext * C, ARegion *region, const rcti *item_rect, void *arg, void *active);
using ButtonSearchListenFn = void (*)(const wmRegionListenerParams *params, void *arg);

using BlockHandleFunc = void (*)(bContext *C, void *arg, int event);

/* -------------------------------------------------------------------- */
/** \name Custom Interaction
 *
 * Sometimes it's useful to create data that remains available
 * while the user interacts with a button.
 *
 * A common case is dragging a number button or slider
 * however this could be used in other cases too.
 * \{ */

struct BlockInteraction_Params {
  /**
   * When true, this interaction is not modal
   * (user clicking on a number button arrows or pasting a value for example).
   */
  bool is_click;
  /**
   * Array of unique event ID's (values from #Button.retval).
   * There may be more than one for multi-button editing (see #BUT_DRAG_MULTI).
   */
  int *unique_retval_ids;
  uint unique_retval_ids_len;
};

/** Returns 'user_data', freed by #uiBlockInteractionEndFn. */
using BlockInteractionBeginFn = void *(*)(bContext * C,
                                          const BlockInteraction_Params *params,
                                          void *arg1);
using BlockInteractionEndFn = void (*)(bContext *C,
                                       const BlockInteraction_Params *params,
                                       void *arg1,
                                       void *user_data);
using BlockInteractionUpdateFn = void (*)(bContext *C,
                                          const BlockInteraction_Params *params,
                                          void *arg1,
                                          void *user_data);

struct BlockInteraction_CallbackData {
  BlockInteractionBeginFn begin_fn;
  BlockInteractionEndFn end_fn;
  BlockInteractionUpdateFn update_fn;
  void *arg1;
};

void block_interaction_set(Block *block, BlockInteraction_CallbackData *callbacks);

/** \} */

/* `interface_query.cc` */

bool but_has_quick_tooltip(const Button *but);
bool but_is_tool(const Button *but);
/** File selectors are exempt from UTF8 checks. */
bool but_is_utf8(const Button *but);
#define button_is_decorator(but) ((but)->type == ButtonType::Decorator)

bool block_is_empty_ex(const Block *block, bool skip_title);
bool block_is_empty(const Block *block);
bool block_can_add_separator(const Block *block);
/**
 * Return true when the block has a default button.
 * Use this for popups to detect when pressing "Return" will run an action.
 */
bool block_has_active_default_button(const Block *block);

/**
 * Find a button under the mouse cursor, ignoring non-interactive ones (like labels). Holding Ctrl
 * over a label button that can be Ctrl-Clicked to turn into an edit button will return that.
 * Labels that are only interactive for the sake of displaying a tooltip are ignored too.
 */
Button *but_find_mouse_over(const ARegion *region, const wmEvent *event) ATTR_WARN_UNUSED_RESULT;

uiList *list_find_mouse_over(const ARegion *region, const wmEvent *event);

/* `interface_region_menu_popup.cc` */

/**
 * Popup Menus
 *
 * Functions used to create popup menus. For more extended menus the
 * popup_menu_begin/End functions can be used to define own items with
 * the uiItem functions in between. If it is a simple confirmation menu
 * or similar, popups can be created with a single function call.
 */
struct PopupMenu;

PopupMenu *popup_menu_begin(bContext *C, const char *title, int icon) ATTR_NONNULL();
/**
 * Directly create a popup menu that is not refreshed on redraw.
 *
 * Only return handler, and set optional title.
 * \param block_name: Assigned to Block.name (useful info for debugging).
 */
PopupMenu *popup_menu_begin_ex(bContext *C, const char *title, const char *block_name, int icon)
    ATTR_NONNULL();
/**
 * Set the whole structure to work.
 */
void popup_menu_end(bContext *C, PopupMenu *pup);
bool popup_menu_end_or_cancel(bContext *C, PopupMenu *pup);
Layout *popup_menu_layout(PopupMenu *pup);

void popup_menu_reports(bContext *C, ReportList *reports) ATTR_NONNULL();
wmOperatorStatus popup_menu_invoke(bContext *C, const char *idname, ReportList *reports)
    ATTR_NONNULL(1, 2);

/**
 * If \a block is displayed in a popup menu, tag it for closing.
 * \param is_cancel: If set to true, the popup will be closed as being cancelled (e.g. when
 *                   pressing escape) as opposed to being handled successfully.
 */
void popup_menu_close(const Block *block, bool is_cancel = false);
/**
 * Version of #popup_menu_close() that can be called on a button contained in a popup menu
 * block. Convenience since the block may not be available.
 */
void popup_menu_close_from_but(const Button *but, bool is_cancel = false);

/**
 * Allow setting menu return value from externals.
 * E.g. WM might need to do this for exiting files correctly.
 */
void popup_menu_retval_set(const Block *block, int retval, bool enable);
/**
 * Set a dummy panel in the popup `block` to support using layout panels.
 * \param idname: Active #PanelType::idname or #OperatorType::idname in the popup for persistent
 * layout panel state storage at runtime.
 */
void popup_dummy_panel_set(ARegion *region, Block *block, StringRef idname);
/**
 * Gets the persistent layout panels state storage in popups.
 * \param idname: Active #PanelType::idname or #OperatorType::idname in the popup.
 */
ListBaseT<LayoutPanelState> &popup_persistent_layout_panel_states(StringRef idname);
/**
 * Setting the button makes the popup open from the button instead of the cursor.
 */
void popup_menu_but_set(PopupMenu *pup, ARegion *butregion, Button *but);

/* `interface_region_popover.cc` */

struct Popover;

wmOperatorStatus popover_panel_invoke(bContext *C,
                                      const char *idname,
                                      bool keep_open,
                                      ReportList *reports);

/**
 * Only return handler, and set optional title.
 *
 * \param from_active_button: Use the active button for positioning,
 * use when the popover is activated from an operator instead of directly from the button.
 */
Popover *popover_begin(bContext *C, int ui_menu_width, bool from_active_button) ATTR_NONNULL(1);
/**
 * Set the whole structure to work.
 */
void popover_end(bContext *C, Popover *pup, wmKeyMap *keymap);
Layout *popover_layout(Popover *pup);
void popover_once_clear(Popover *pup);

/* `interface_region_menu_pie.cc` */

/* Pie menus */
struct PieMenu;

wmOperatorStatus pie_menu_invoke(bContext *C, const char *idname, const wmEvent *event);

PieMenu *pie_menu_begin(bContext *C, const char *title, int icon, const wmEvent *event)
    ATTR_NONNULL();
void pie_menu_end(bContext *C, PieMenu *pie);
Layout *pie_menu_layout(PieMenu *pie);

/* `interface_region_menu_popup.cc` */

/* Popup Blocks
 *
 * Functions used to create popup blocks. These are like popup menus
 * but allow using all button types and creating their own layout. */
using BlockCreateFunc = Block *(*)(bContext * C, ARegion *region, void *arg1);
using BlockCancelFunc = void (*)(bContext *C, void *arg1);

void popup_block_invoke(bContext *C, BlockCreateFunc func, void *arg, FreeArgFunc arg_free);
/**
 * \param can_refresh: When true, the popup may be refreshed (updated after creation).
 * \note It can be useful to disable refresh (even though it will work)
 * as this exits text fields which can be disruptive if refresh isn't needed.
 */
void popup_block_invoke_ex(
    bContext *C, BlockCreateFunc func, void *arg, FreeArgFunc arg_free, bool can_refresh);
void popup_block_ex(bContext *C,
                    BlockCreateFunc func,
                    BlockHandleFunc popup_func,
                    BlockCancelFunc cancel_func,
                    void *arg,
                    wmOperator *op);

/**
 * Return true when #popup_block_template_confirm and related functions are supported.
 */
bool popup_block_template_confirm_is_supported(const Block *block);
/**
 * Create confirm & cancel buttons in a popup using callback functions.
 */
void popup_block_template_confirm(Block *block,
                                  bool cancel_default,
                                  FunctionRef<Button *()> confirm_fn,
                                  FunctionRef<Button *()> cancel_fn);
/**
 * Create confirm & cancel buttons in a popup using an operator.
 *
 * \param confirm_text: The text to confirm, null for default text or an empty string to hide.
 * \param cancel_text: The text to cancel, null for default text or an empty string to hide.
 * \param r_ptr: The pointer for operator properties, set a "confirm" button has been created.
 */
void popup_block_template_confirm_op(Layout *layout,
                                     wmOperatorType *ot,
                                     std::optional<StringRef> confirm_text,
                                     std::optional<StringRef> cancel_text,
                                     const int icon,
                                     bool cancel_default,
                                     PointerRNA *r_ptr);

#if 0 /* UNUSED */
void uiPupBlockOperator(bContext *C,
                        uiBlockCreateFunc func,
                        wmOperator *op,
                        wm::OpCallContext opcontext);
#endif

void popup_block_close(bContext *C, wmWindow *win, Block *block);

bool popup_block_name_exists(const bScreen *screen, StringRef name);

/* Blocks
 *
 * Functions for creating, drawing and freeing blocks. A Block is a
 * container of buttons and used for various purposes.
 *
 * Begin/Define Buttons/End/Draw is the typical order in which these
 * function should be called, though for popup blocks Draw is left out.
 * Freeing blocks is done by the screen/ module automatically.
 */

Block *block_begin(const bContext *C, ARegion *region, std::string name, EmbossType emboss);
Block *block_begin(const bContext *C,
                   Scene *scene,
                   wmWindow *window,
                   ARegion *region,
                   std::string name,
                   EmbossType emboss);
void block_end_ex(const bContext *C,
                  Main *bmain,
                  wmWindow *window,
                  Scene *scene,
                  ARegion *region,
                  Depsgraph *depsgraph,
                  Block *block,
                  const int xy[2] = nullptr,
                  int r_xy[2] = nullptr);
void block_end(const bContext *C, Block *block);
/**
 * Uses local copy of style, to scale things down, and allow widgets to change stuff.
 */
void block_draw(const bContext *C, Block *block);
void blocklist_update_window_matrix(const bContext *C, const ListBaseT<ui::Block> *lb);
void blocklist_update_view_for_buttons(const bContext *C, const ListBaseT<ui::Block> *lb);
void blocklist_draw(const bContext *C, const ListBaseT<ui::Block> *lb);
void block_update_from_old(const bContext *C, Block *block);

enum {
  BLOCK_THEME_STYLE_REGULAR = 0,
  BLOCK_THEME_STYLE_POPUP = 1,
};
void block_theme_style_set(Block *block, char theme_style);
EmbossType block_emboss_get(Block *block);
void block_emboss_set(Block *block, EmbossType emboss);
bool block_is_search_only(const Block *block);
/**
 * Use when a block must be searched to give accurate results
 * for the whole region but shouldn't be displayed.
 */
void block_set_search_only(Block *block, bool search_only);

/**
 * Used for operator presets.
 */
void block_set_active_operator(Block *block, wmOperator *op, const bool free);

/**
 * Can be called with C==NULL.
 */
void block_free(const bContext *C, Block *block);

void block_listen(const Block *block, const wmRegionListenerParams *listener_params);

/**
 * Can be called with C==NULL.
 */
void blocklist_free(const bContext *C, ARegion *region);
void blocklist_free_inactive(const bContext *C, ARegion *region);

/**
 * Is called by notifier.
 */
void UI_screen_free_active_but_highlight(const bContext *C, bScreen *screen);
void UI_region_free_active_but_all(bContext *C, ARegion *region);

void block_region_set(Block *block, ARegion *region);

void block_lock_set(Block *block, bool val, const char *lockstr);
void block_lock_clear(Block *block);

#define UI_BUTTON_SECTION_MERGE_DISTANCE (UI_UNIT_X * 3)
/* Separator line between regions if the #uiButtonSectionsAlign is not #None. */
#define UI_BUTTON_SECTION_SEPERATOR_LINE_WITH (U.pixelsize * 2)

enum class ButtonSectionsAlign : int8_t { None = 1, Top, Bottom };
/**
 * Draw a background with rounded corners behind each visual group of buttons. The visual groups
 * are separated by spacer buttons (#Layout::separator_spacer()). Button groups that
 * are closer than #UI_BUTTON_SECTION_MERGE_DISTANCE will be merged into one visual section. If the
 * group is closer than that to a region edge, it will also be extended to that, and the rounded
 * corners will be removed on that edge.
 *
 * \note This currently only works well for horizontal, header like regions.
 */
void region_button_sections_draw(const ARegion *region,
                                 int /*THemeColorID*/ colorid,
                                 ButtonSectionsAlign align);
bool region_button_sections_is_inside_x(const ARegion *region, const int mval_x);

/**
 * Automatic aligning, horizontal or vertical.
 */
void block_align_begin(Block *block);
void block_align_end(Block *block);

/** Block bounds/position calculation. */
enum BlockBoundsCalc {
  BLOCK_BOUNDS_NONE = 0,
  BLOCK_BOUNDS = 1,
  BLOCK_BOUNDS_TEXT,
  BLOCK_BOUNDS_POPUP_MOUSE,
  BLOCK_BOUNDS_POPUP_MENU,
  BLOCK_BOUNDS_POPUP_CENTER,
  BLOCK_BOUNDS_PIE_CENTER,
};

/**
 * Used for various cases.
 */
void block_bounds_set_normal(Block *block, int addval);
/**
 * Used for pull-downs.
 */
void block_bounds_set_text(Block *block, int addval);
/**
 * Used for block popups.
 */
void block_bounds_set_popup(Block *block, int addval, const int bounds_offset[2]);
/**
 * Used for menu popups.
 */
void block_bounds_set_menu(Block *block, int addval, const int bounds_offset[2]);
/**
 * Used for centered popups, i.e. splash.
 */
void block_bounds_set_centered(Block *block, int addval);
void block_bounds_set_explicit(Block *block, int minx, int miny, int maxx, int maxy);

int blocklist_min_y_get(ListBaseT<ui::Block> *lb);

void block_direction_set(Block *block, char direction);
/**
 * This call escapes if there's alignment flags.
 */
void block_flag_enable(Block *block, int flag);
void block_flag_disable(Block *block, int flag);
void block_translate(Block *block, float x, float y);

int button_return_value_get(Button *but);

Button *button_active_drop_name_button(const bContext *C);
/**
 * Returns true if highlighted button allows drop of names.
 * called in region context.
 */
bool button_active_drop_name(const bContext *C);
bool button_active_drop_color(bContext *C);

void button_flag_enable(Button *but, int flag);
void button_flag_disable(Button *but, int flag);
bool button_flag_is_set(Button *but, int flag);
void button_flag2_enable(Button *but, int flag);

void button_drawflag_enable(Button *but, int flag);
void button_drawflag_disable(Button *but, int flag);

void button_dragflag_enable(Button *but, int flag);
void button_dragflag_disable(Button *but, int flag);

void button_disable(Button *but, const char *disabled_hint);

void button_type_set_menu_from_pulldown(Button *but);

/**
 * Sets the button's color, normally only used to recolor the icon. In the
 * special case of ButtonType::Label without icon this is used as text color.
 */
void button_color_set(Button *but, const uchar color[4]);

bool button_is_color_gamma(Button &but);
const ColorManagedDisplay *button_cm_display_get(Button &but);

/**
 * Set at hint that describes the expected value when empty.
 */
void button_placeholder_set(Button *but, StringRef placeholder_text);

/**
 * Special button case, only draw it when used actively, for outliner etc.
 *
 * Needed for temporarily rename buttons, such as in outliner or file-select,
 * they should keep calling #uiDefBut to keep them alive.
 * \return false when button removed.
 */
bool button_active_only_ex(
    const bContext *C, ARegion *region, Block *block, Button *but, bool remove_on_failure);
bool button_active_only(const bContext *C, ARegion *region, Block *block, Button *but);
/**
 * \warning This must run after other handlers have been added,
 * otherwise the handler won't be removed, see: #71112.
 */
bool block_active_only_flagged_buttons(const bContext *C, ARegion *region, Block *block);

/**
 * Simulate button click.
 */
void button_execute(const bContext *C, ARegion *region, Button *but);

std::optional<std::string> button_online_manual_id(const Button *but) ATTR_WARN_UNUSED_RESULT;
std::optional<std::string> button_online_manual_id_from_active(const bContext *C)
    ATTR_WARN_UNUSED_RESULT;
bool button_is_userdef(const Button *but);

/* Buttons
 *
 * Functions to define various types of buttons in a block. Postfixes:
 * - F: float
 * - I: int
 * - S: short
 * - C: char
 * - R: RNA
 * - O: operator */

Button *uiDefBut(Block *block,
                 ButtonTypeWithPointerType but_and_ptr_type,
                 StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 void *poin,
                 float min,
                 float max,
                 std::optional<StringRef> tip);
Button *uiDefButF(Block *block,
                  ButtonType type,
                  StringRef str,
                  int x,
                  int y,
                  short width,
                  short height,
                  float *poin,
                  float min,
                  float max,
                  std::optional<StringRef> tip);
Button *uiDefButI(Block *block,
                  ButtonType type,
                  StringRef str,
                  int x,
                  int y,
                  short width,
                  short height,
                  int *poin,
                  float min,
                  float max,
                  std::optional<StringRef> tip);
Button *uiDefButBitI(Block *block,
                     ButtonType type,
                     int bit,
                     StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     int *poin,
                     float min,
                     float max,
                     std::optional<StringRef> tip);
Button *uiDefButS(Block *block,
                  ButtonType type,
                  StringRef str,
                  int x,
                  int y,
                  short width,
                  short height,
                  short *poin,
                  float min,
                  float max,
                  std::optional<StringRef> tip);
Button *uiDefButBitS(Block *block,
                     ButtonType type,
                     int bit,
                     StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     short *poin,
                     float min,
                     float max,
                     std::optional<StringRef> tip);
Button *uiDefButC(Block *block,
                  ButtonType type,
                  StringRef str,
                  int x,
                  int y,
                  short width,
                  short height,
                  char *poin,
                  float min,
                  float max,
                  std::optional<StringRef> tip);
Button *uiDefButBitC(Block *block,
                     ButtonType type,
                     int bit,
                     StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     char *poin,
                     float min,
                     float max,
                     std::optional<StringRef> tip);
Button *uiDefButR(Block *block,
                  ButtonType type,
                  std::optional<StringRef> str,
                  int x,
                  int y,
                  short width,
                  short height,
                  PointerRNA *ptr,
                  StringRefNull propname,
                  int index,
                  float min,
                  float max,
                  std::optional<StringRef> tip);
Button *uiDefButR_prop(Block *block,
                       ButtonType type,
                       std::optional<StringRef> str,
                       int x,
                       int y,
                       short width,
                       short height,
                       PointerRNA *ptr,
                       PropertyRNA *prop,
                       int index,
                       float min,
                       float max,
                       std::optional<StringRef> tip);
Button *uiDefButO(Block *block,
                  ButtonType type,
                  StringRefNull opname,
                  wm::OpCallContext opcontext,
                  const std::optional<StringRef> str,
                  int x,
                  int y,
                  short width,
                  short height,
                  std::optional<StringRef> tip);
Button *uiDefButO_ptr(Block *block,
                      ButtonType type,
                      wmOperatorType *ot,
                      wm::OpCallContext opcontext,
                      StringRef str,
                      int x,
                      int y,
                      short width,
                      short height,
                      std::optional<StringRef> tip);

Button *uiDefIconBut(Block *block,
                     ButtonTypeWithPointerType but_and_ptr_type,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     void *poin,
                     float min,
                     float max,
                     std::optional<StringRef> tip);
Button *uiDefIconButI(Block *block,
                      ButtonType type,
                      int icon,
                      int x,
                      int y,
                      short width,
                      short height,
                      int *poin,
                      float min,
                      float max,
                      std::optional<StringRef> tip);
Button *uiDefIconButBitI(Block *block,
                         ButtonType type,
                         int bit,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         int *poin,
                         float min,
                         float max,
                         std::optional<StringRef> tip);
Button *uiDefIconButS(Block *block,
                      ButtonType type,
                      int icon,
                      int x,
                      int y,
                      short width,
                      short height,
                      short *poin,
                      float min,
                      float max,
                      std::optional<StringRef> tip);
Button *uiDefIconButBitS(Block *block,
                         ButtonType type,
                         int bit,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         short *poin,
                         float min,
                         float max,
                         std::optional<StringRef> tip);
Button *uiDefIconButBitC(Block *block,
                         ButtonType type,
                         int bit,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         char *poin,
                         float min,
                         float max,
                         std::optional<StringRef> tip);
Button *uiDefIconButR(Block *block,
                      ButtonType type,
                      int icon,
                      int x,
                      int y,
                      short width,
                      short height,
                      PointerRNA *ptr,
                      StringRefNull propname,
                      int index,
                      float min,
                      float max,
                      std::optional<StringRef> tip);
Button *uiDefIconButR_prop(Block *block,
                           ButtonType type,
                           int icon,
                           int x,
                           int y,
                           short width,
                           short height,
                           PointerRNA *ptr,
                           PropertyRNA *prop,
                           int index,
                           float min,
                           float max,
                           std::optional<StringRef> tip);
Button *uiDefIconButO(Block *block,
                      ButtonType type,
                      StringRefNull opname,
                      wm::OpCallContext opcontext,
                      int icon,
                      int x,
                      int y,
                      short width,
                      short height,
                      std::optional<StringRef> tip);
Button *uiDefIconButO_ptr(Block *block,
                          ButtonType type,
                          wmOperatorType *ot,
                          wm::OpCallContext opcontext,
                          int icon,
                          int x,
                          int y,
                          short width,
                          short height,
                          std::optional<StringRef> tip);
Button *uiDefIconPreviewBut(Block *block,
                            ButtonType type,
                            int icon,
                            int x,
                            int y,
                            short width,
                            short height,
                            void *poin,
                            float min,
                            float max,
                            std::optional<StringRef> tip);
Button *uiDefButImage(
    Block *block, void *imbuf, int x, int y, short width, short height, const uchar color[4]);
Button *uiDefButAlert(Block *block, AlertIcon icon, int x, int y, short width, short height);
/** Button containing both string label and icon. */
Button *uiDefIconTextBut(Block *block,
                         ButtonTypeWithPointerType but_and_ptr_type,
                         int icon,
                         StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         void *poin,
                         std::optional<StringRef> tip);
Button *uiDefIconTextButI(Block *block,
                          ButtonType type,
                          int icon,
                          StringRef str,
                          int x,
                          int y,
                          short width,
                          short height,
                          int *poin,
                          std::optional<StringRef> tip);
Button *uiDefIconTextButS(Block *block,
                          ButtonType type,
                          int icon,
                          StringRef str,
                          int x,
                          int y,
                          short width,
                          short height,
                          short *poin,
                          std::optional<StringRef> tip);
Button *uiDefIconTextButR(Block *block,
                          ButtonType type,
                          int icon,
                          std::optional<StringRefNull> str,
                          int x,
                          int y,
                          short width,
                          short height,
                          PointerRNA *ptr,
                          StringRefNull propname,
                          int index,
                          std::optional<StringRef> tip);
Button *uiDefIconTextButR_prop(Block *block,
                               ButtonType type,
                               int icon,
                               std::optional<StringRef> str,
                               int x,
                               int y,
                               short width,
                               short height,
                               PointerRNA *ptr,
                               PropertyRNA *prop,
                               int index,
                               float min,
                               float max,
                               std::optional<StringRef> tip);
Button *uiDefIconTextButO(Block *block,
                          ButtonType type,
                          StringRefNull,
                          wm::OpCallContext opcontext,
                          int icon,
                          StringRef str,
                          int x,
                          int y,
                          short width,
                          short height,
                          std::optional<StringRef> tip);
Button *uiDefIconTextButO_ptr(Block *block,
                              ButtonType type,
                              wmOperatorType *ot,
                              wm::OpCallContext opcontext,
                              int icon,
                              StringRef str,
                              int x,
                              int y,
                              short width,
                              short height,
                              std::optional<StringRef> tip);

void button_retval_set(Button *but, int retval);

void button_operator_set(Button *but,
                         wmOperatorType *optype,
                         wm::OpCallContext opcontext,
                         const PointerRNA *opptr = nullptr);
/**
 * Disable calling operators from \a but in button handling. Useful to attach an operator to a
 * button for tooltips, "Assign Shortcut", etc. without actually making the button execute the
 * operator.
 */
void button_operator_set_never_call(Button *but);

/** For passing inputs to ButO buttons. */
PointerRNA *button_operator_ptr_ensure(Button *but);

void button_context_ptr_set(Block *block, Button *but, StringRef name, const PointerRNA *ptr);
void button_context_int_set(Block *block, Button *but, StringRef name, int64_t value);
const PointerRNA *button_context_ptr_get(const Button *but,
                                         StringRef name,
                                         const StructRNA *type = nullptr);
std::optional<StringRefNull> button_context_string_get(const Button *but, StringRef name);
std::optional<int64_t> button_context_int_get(const Button *but, StringRef name);
const bContextStore *button_context_get(const Button *but);

void button_unit_type_set(Button *but, int unit_type);
int button_unit_type_get(const Button *but);

std::optional<EnumPropertyItem> button_rna_enum_item_get(bContext &C, Button &but);

std::string button_string_get_rna_property_identifier(const Button &but);
std::string button_string_get_rna_struct_identifier(const Button &but);
std::string button_string_get_label(Button &but);
std::string button_context_menu_title_from_button(Button &but);
/**
 * Query the result of #Button::tip_label_func().
 * Meant to allow overriding the label to be displayed in the tool-tip.
 */
std::string button_string_get_tooltip_label(const Button &but);
std::string button_string_get_rna_label(Button &but);
/** Context specified in `CTX_*_` macros are just unreachable! */
std::string button_string_get_rna_label_context(const Button &but);
std::string button_string_get_tooltip(bContext &C, Button &but);
std::string button_string_get_rna_tooltip(bContext &C, Button &but);
/** Buttons assigned to an operator (common case). */
std::string button_string_get_operator_keymap(bContext &C, Button &but);
/** Use for properties that are bound to one of the context cycle, etc. keys. */
std::string button_string_get_property_keymap(bContext &C, Button &but);

std::string button_extra_icon_string_get_label(const ButtonExtraOpIcon &extra_icon);
std::string button_extra_icon_string_get_tooltip(bContext &C, const ButtonExtraOpIcon &extra_icon);
std::string button_extra_icon_string_get_operator_keymap(const bContext &C,
                                                         const ButtonExtraOpIcon &extra_icon);

/**
 * Special Buttons
 *
 * Buttons with a more specific purpose:
 * - MenuBut: buttons that popup a menu (in headers usually).
 * - PulldownBut: like MenuBut, but creating a Block (for compatibility).
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
  TEMPLATE_ID_FILTER_ALL = 0,
  TEMPLATE_ID_FILTER_AVAILABLE = 1,
};

/***************************** ID Utilities *******************************/

int icon_from_id(const ID *id);
/** See: #BKE_report_type_str */
int icon_from_report_type(int type);
int icon_colorid_from_report_type(int type);
int UI_text_colorid_from_report_type(int type);

int icon_from_event_type(short event_type, short event_value);
int icon_from_keymap_item(const wmKeyMapItem *kmi, int r_icon_mod[KM_MOD_NUM]);

Button *uiDefMenuBut(Block *block,
                     MenuCreateFunc func,
                     void *arg,
                     StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     std::optional<StringRef> tip);
Button *uiDefIconTextMenuBut(Block *block,
                             MenuCreateFunc func,
                             void *arg,
                             int icon,
                             StringRef str,
                             int x,
                             int y,
                             short width,
                             short height,
                             std::optional<StringRef> tip);
Button *uiDefIconMenuBut(Block *block,
                         MenuCreateFunc func,
                         void *arg,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         std::optional<StringRef> tip);

Button *uiDefBlockBut(Block *block,
                      BlockCreateFunc func,
                      void *arg,
                      StringRef str,
                      int x,
                      int y,
                      short width,
                      short height,
                      std::optional<StringRef> tip);
Button *uiDefBlockButN(Block *block,
                       BlockCreateFunc func,
                       void *argN,
                       StringRef str,
                       int x,
                       int y,
                       short width,
                       short height,
                       std::optional<StringRef> tip,
                       ButtonArgNFree func_argN_free_fn = MEM_freeN,
                       ButtonArgNCopy func_argN_copy_fn = MEM_dupallocN);

/**
 * Block button containing icon.
 */
Button *uiDefIconBlockBut(Block *block,
                          BlockCreateFunc func,
                          void *arg,
                          int icon,
                          int x,
                          int y,
                          short width,
                          short height,
                          std::optional<StringRef> tip);

/**
 * \param arg: A pointer to string/name, use #button_func_search_set() below to make this work.
 */
Button *uiDefSearchBut(Block *block,
                       void *arg,
                       int icon,
                       int maxncpy,
                       int x,
                       int y,
                       short width,
                       short height,
                       std::optional<StringRef> tip);
/**
 * Same parameters as for #uiDefSearchBut, with additional operator type and properties,
 * used by callback to call again the right op with the right options (properties values).
 */
Button *uiDefSearchButO_ptr(Block *block,
                            wmOperatorType *ot,
                            IDProperty *properties,
                            void *arg,
                            int icon,
                            int maxncpy,
                            int x,
                            int y,
                            short width,
                            short height,
                            std::optional<StringRef> tip);

/** For #uiDefAutoButsRNA. */
enum eButLabelAlign {
  /** Keep current layout for aligning label with property button. */
  BUT_LABEL_ALIGN_NONE,
  /** Align label and property button vertically. */
  BUT_LABEL_ALIGN_COLUMN,
  /** Split layout into a column for the label and one for property button. */
  BUT_LABEL_ALIGN_SPLIT_COLUMN,
};

/** Return info for uiDefAutoButsRNA. */
enum AutoPropButsReturn {
  /** Returns when no buttons were added */
  PROP_BUTS_NONE_ADDED = 1 << 0,
  /** Returned when any property failed the custom check callback (check_prop) */
  PROP_BUTS_ANY_FAILED_CHECK = 1 << 1,
};

ENUM_OPERATORS(AutoPropButsReturn);

/**
 * \param button_type_override \parblock
 * Overrides the default button type defined for some properties:
 * - Int/Float properties allows #ButtonType::Num or #ButtonType::NumSlider.
 * - Enum properties allows #ButtonType::Menu or #ButtonType::SearchMenu.
 * - String properties allows #ButtonType::Text or #ButtonType::SearchMenu.
 *
 * This has no effect on other property types.
 * \endparblock
 */
Button *uiDefAutoButR(Block *block,
                      PointerRNA *ptr,
                      PropertyRNA *prop,
                      int index,
                      std::optional<StringRef> name,
                      int icon,
                      int x,
                      int y,
                      int width,
                      int height,
                      std::optional<ButtonType> button_type_override = std::nullopt);
void uiDefAutoButsArrayR(Block *block,
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
 * \param prop_activate_init: Property to activate on initial popup (#BUT_ACTIVATE_ON_INIT).
 */
AutoPropButsReturn uiDefAutoButsRNA(Layout *layout,
                                    PointerRNA *ptr,
                                    bool (*check_prop)(PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       void *user_data),
                                    void *user_data,
                                    PropertyRNA *prop_activate_init,
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
void button_func_identity_compare_set(Button *but, ButtonIdentityCompareFunc cmp_fn);

/**
 * Public function exported for functions that use #ButtonType::SearchMenu.
 *
 * Use inside searchfunc to add items.
 *
 * \param items: Stores the items.
 * \param name: Text to display for the item.
 * \param poin: Opaque pointer (for use by the caller).
 * \param iconid: The icon, #ICON_NONE for no icon.
 * \param but_flag: Button flags (#Button.flag) indicating the state of the item, typically
 *                  #BUT_DISABLED, #BUT_INACTIVE or #BUT_HAS_SEP_CHAR.
 *
 * \return false if there is nothing to add.
 */
bool search_item_add(SearchItems *items,
                     StringRef name,
                     void *poin,
                     int iconid,
                     int but_flag,
                     uint8_t name_prefix_offset);

/**
 * \note The item-pointer (referred to below) is a per search item user pointer
 * passed to #search_item_add (stored in  #SearchItems.pointers).
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
void button_func_search_set(Button *but,
                            ButtonSearchCreateFn search_create_fn,
                            ButtonSearchUpdateFn search_update_fn,
                            void *arg,
                            bool free_arg,
                            FreeArgFunc search_arg_free_fn,
                            ButtonHandleFunc search_exec_fn,
                            void *active);
void button_func_search_set_context_menu(Button *but, ButtonSearchContextMenuFn context_menu_fn);
void button_func_search_set_tooltip(Button *but, ButtonSearchTooltipFn tooltip_fn);
void button_func_search_set_listen(Button *but, ButtonSearchListenFn listen_fn);
/**
 * \param search_sep_string: when not NULL, this string is used as a separator,
 * showing the icon and highlighted text after the last instance of this string.
 */
void button_func_search_set_sep_string(Button *but, const char *search_sep_string);
void button_func_search_set_results_are_suggestions(Button *but, bool value);

#define UI_SEARCHBOX_BOUNDS (6.0f * UI_SCALE_FAC)
#define UI_SEARCHBOX_TRIA_H (12.0f * UI_SCALE_FAC)
/**
 * Height in pixels, it's using hard-coded values still.
 */
int searchbox_size_y();
int searchbox_size_x();
/**
 * Guess a good width for the search box based on the searchable items.
 *
 * \note When used with a menu that does full refreshes, it might be beneficial to cache this size
 * because recomputing it is potentially expensive.
 */
int searchbox_size_x_guess(const bContext *C, const ButtonSearchUpdateFn update_fn, void *arg);
/**
 * Check if a string is in an existing search box.
 */
int search_items_find_index(const SearchItems *items, const char *name);

/**
 * Adds a hint to the button which draws right aligned, grayed out and never clipped.
 */
void button_hint_drawstr_set(Button *but, const char *string);
void button_icon_indicator_number_set(Button *but, const int indicator_number);
void button_icon_indicator_set(Button *but, const char *string);
void button_icon_indicator_color_set(Button *but, const uchar color[4]);

void button_node_link_set(Button *but, bNodeSocket *socket, const float draw_color[4]);

void button_number_step_size_set(Button *but, float step_size);
void button_number_precision_set(Button *but, float precision);

void button_number_slider_step_size_set(Button *but, float step_size);
void button_number_slider_precision_set(Button *but, float precision);

void button_label_alpha_factor_set(Button *but, float alpha_factor);

void button_search_preview_grid_size_set(Button *but, int rows, int cols);

void button_view_item_draw_size_set(Button *but,
                                    const std::optional<int> draw_width = std::nullopt,
                                    const std::optional<int> draw_height = std::nullopt);

void block_func_handle_set(Block *block, BlockHandleFunc func, void *arg);
void block_func_set(Block *block, ButtonHandleFunc func, void *arg1, void *arg2);
void block_funcN_set(Block *block,
                     ButtonHandleNFunc funcN,
                     void *argN,
                     void *arg2,
                     ButtonArgNFree func_argN_free_fn = MEM_freeN,
                     ButtonArgNCopy func_argN_copy_fn = MEM_dupallocN);

void button_func_rename_set(Button *but, ButtonHandleRenameFunc func, void *arg1);
void button_func_rename_full_set(Button *but,
                                 std::function<void(std::string &new_name)> rename_full_func);
void button_func_set(Button *but, ButtonHandleFunc func, void *arg1, void *arg2);
void button_funcN_set(Button *but,
                      ButtonHandleNFunc funcN,
                      void *argN,
                      void *arg2,
                      ButtonArgNFree func_argN_free_fn = MEM_freeN,
                      ButtonArgNCopy func_argN_copy_fn = MEM_dupallocN);

void button_func_complete_set(Button *but, ButtonCompleteFunc func, void *arg);

void button_func_drawextra_set(Block *block,
                               std::function<void(const bContext *C, rcti *rect)> func);

void button_func_menu_step_set(Button *but, MenuStepFunc func);

/**
 * When a button displays a menu, hovering another button that can display one will switch to that
 * menu instead. In some cases that's unexpected, so the feature can be disabled here (as in, this
 * button will not spawn its menu on hover and the previously spawned menu will remain open).
 */
void button_menu_disable_hover_open(Button *but);

void button_func_tooltip_set(Button *but, ButtonToolTipFunc func, void *arg, FreeArgFunc free_arg);
/**
 * Enable a tooltip that appears faster than the usual tooltip. If the button has both a quick and
 * a normal tooltip, the quick one is shown first, and expanded to the full one after the usual
 * tooltip delay. Quick tooltips are useful in cases like:
 * - A button doesn't show a label to save space but the label is still relevant. Show the label as
 *   quick tooltip in that case (like the name of tools in a compact, icon only tool-shelf).
 * - The only purpose of a button is to display this tooltip (like a warning icon with the warning
 *   text in the tooltip).
 */
void button_func_quick_tooltip_set(Button *but,
                                   std::function<std::string(const Button *but)> func);

enum TooltipStyle {
  TIP_STYLE_NORMAL = 0, /* Regular text. */
  TIP_STYLE_HEADER,     /* Header text. */
  TIP_STYLE_MONO,       /* Mono-spaced text. */
  TIP_STYLE_IMAGE,      /* Image field. */
  TIP_STYLE_SPACER,     /* Padding to separate sections. */
};

enum TooltipColorID {
  TIP_LC_MAIN = 0, /* Color of primary text. */
  TIP_LC_VALUE,    /* Color for the value of buttons (also shortcuts). */
  TIP_LC_ACTIVE,   /* Color of titles of active enum values. */
  TIP_LC_NORMAL,   /* Color of regular text. */
  TIP_LC_PYTHON,   /* Color of python snippets. */
  TIP_LC_ALERT,    /* Warning text color, eg: why operator can't run. */
  TIP_LC_MAX
};

enum class TooltipImageBackground {
  None = 0,
  Checkerboard_Themed,
  Checkerboard_Fixed,
};

struct TooltipImage {
  ImBuf *ibuf = nullptr;
  short width = 0;
  short height = 0;
  bool premultiplied = false;
  bool border = false;
  bool text_color = false;
  TooltipImageBackground background = TooltipImageBackground::None;
};

void button_func_tooltip_custom_set(Button *but,
                                    ButtonToolTipCustomFunc func,
                                    void *arg,
                                    FreeArgFunc free_arg);

/**
 * \param text: Allocated text (transfer ownership to `data`) or null.
 * \param suffix: Allocated text (transfer ownership to `data`) or null.
 */
void tooltip_text_field_add(TooltipData &data,
                            std::string text,
                            std::string suffix,
                            const TooltipStyle style,
                            const TooltipColorID color_id,
                            const bool is_pad = false);

/**
 * \param image: Image buffer (duplicated, ownership is *not* transferred to `data`).
 * \param image_size: Display size for the image (pixels without UI scale applied).
 */
void tooltip_image_field_add(TooltipData &data, const TooltipImage &image_data);

void tooltip_color_field_add(TooltipData &data,
                             const float4 &color,
                             bool has_alpha,
                             bool is_gamma,
                             const ColorManagedDisplay *display,
                             TooltipColorID color_id);

/**
 * Add Python-related information to the tooltip. The caller is responsible for checking
 * #USER_TOOLTIPS_PYTHON.
 */
void tooltip_uibut_python_add(TooltipData &data,
                              bContext &C,
                              Button &but,
                              ButtonExtraOpIcon *extra_icon);

/**
 * Recreate tool-tip (use to update dynamic tips)
 */
void button_tooltip_refresh(bContext *C, Button *but);
/**
 * Removes tool-tip timer from active but
 * (meaning tool-tip is disabled until it's re-enabled again).
 */
void button_tooltip_timer_remove(bContext *C, Button *but);

bool textbutton_activate_rna(const bContext *C,
                             ARegion *region,
                             const void *rna_poin_data,
                             const char *rna_prop_id);
bool textbutton_activate_but(const bContext *C, Button *actbut);

/**
 * push a new event onto event queue to activate the given button
 * (usually a text-field) upon entering a popup
 */
void button_focus_on_enter_event(wmWindow *win, Button *but);

void button_func_hold_set(Button *but, ButtonHandleHoldFunc func, void *argN);

PointerRNA *button_extra_operator_icon_add(Button *but,
                                           StringRefNull opname,
                                           wm::OpCallContext opcontext,
                                           int icon);
wmOperatorType *button_extra_operator_icon_optype_get(const ButtonExtraOpIcon *extra_icon);
PointerRNA *button_extra_operator_icon_opptr_get(const ButtonExtraOpIcon *extra_icon);

/**
 * Get the scaled size for a preview button (typically #UI_BTyPE_PREVIEW_TILE) based on \a
 * size_px plus padding.
 */
int preview_tile_size_x(const int size_px = 96);
int preview_tile_size_y(const int size_px = 96);
int preview_tile_size_y_no_label(const int size_px = 96);

/* Autocomplete
 *
 * Tab complete helper functions, for use in uiButCompleteFunc callbacks.
 * Call begin once, then multiple times do_name with all possibilities,
 * and finally end to finish and get the completed name. */

struct AutoComplete;

#define AUTOCOMPLETE_NO_MATCH 0
#define AUTOCOMPLETE_FULL_MATCH 1
#define AUTOCOMPLETE_PARTIAL_MATCH 2

AutoComplete *autocomplete_begin(const char *startname, size_t maxncpy);
void autocomplete_update_name(AutoComplete *autocpl, StringRef name);
int autocomplete_end(AutoComplete *autocpl, char *autoname);

/* Button drag-data (interface_drag.cc).
 *
 * Functions to set drag data for buttons. This enables dragging support, whereby the drag data is
 * "dragged", not the button itself. */

void button_drag_set_id(Button *but, ID *id);
/**
 * Set an image to display while dragging. This works for any drag type (`WM_DRAG_XXX`).
 * Not to be confused with #button_drag_set_image(), which sets up dragging of an image.
 *
 * Sets #BUT_DRAG_FULL_BUT so the full button can be dragged.
 */
void button_drag_attach_image(Button *but, const ImBuf *imb, float scale);

/**
 * Sets #BUT_DRAG_FULL_BUT so the full button can be dragged.
 * \param asset: May be passed from a temporary variable, drag data only stores a copy of this.
 * \param icon: Small icon that will be drawn while dragging.
 * \param preview_icon: Bigger preview size icon that will be drawn while dragging instead of \a
 * icon.
 */
void button_drag_set_asset(Button *but,
                           const asset_system::AssetRepresentation *asset,
                           const AssetImportSettings &import_settings,
                           int icon,
                           int preview_icon);

void button_drag_set_rna(Button *but, PointerRNA *ptr);
/**
 * Enable dragging a path from this button.
 * \param path: The path to drag. The passed string may be destructed, button keeps a copy.
 */
void button_drag_set_path(Button *but, const char *path);
void button_drag_set_name(Button *but, const char *name);

/**
 * Sets #BUT_DRAG_FULL_BUT so the full button can be dragged.
 * \param path: The path to drag. The passed string may be destructed, button keeps a copy.
 */
void button_drag_set_image(Button *but, const char *path, int icon, const ImBuf *imb, float scale);

/* Panels
 *
 * Functions for creating, freeing and drawing panels. The API here
 * could use a good cleanup, though how they will function in 2.5 is
 * not clear yet so we postpone that. */

void panels_begin(const bContext *C, ARegion *region);
void panels_end(const bContext *C, ARegion *region, int *r_x, int *r_y);
/**
 * Draw panels, selected (panels currently being dragged) on top.
 */
void panels_draw(const bContext *C, ARegion *region);

Panel *panel_find_by_type(ListBaseT<Panel> *lb, const PanelType *pt);
/**
 * \note \a panel should be return value from #panel_find_by_type and can be NULL.
 */
Panel *panel_begin(ARegion *region,
                   ListBaseT<Panel> *lb,
                   Block *block,
                   PanelType *pt,
                   Panel *panel,
                   bool *r_open);
/**
 * Create the panel header button group, used to mark which buttons are part of
 * panel headers for the panel search process that happens later. This Should be
 * called before adding buttons for the panel's header layout.
 */
void panel_header_buttons_begin(Panel *panel);
/**
 * Finish the button group for the panel header to avoid putting panel body buttons in it.
 */
void panel_header_buttons_end(Panel *panel);
void panel_end(Panel *panel, int width, int height);

/** Set the name that should be drawn in the UI. Should be a translated string. */
void panel_drawname_set(Panel *panel, StringRef name);

/**
 * Set a context for this entire panel and its current layout. This should be used whenever panel
 * callbacks that are called outside of regular drawing might require context. Currently it affects
 * the #PanelType.reorder callback only.
 */
void panel_context_pointer_set(Panel *panel, const char *name, PointerRNA *ptr);

/**
 * Get the panel's expansion state, taking into account
 * expansion set from property search if it applies.
 */
bool panel_is_closed(const Panel *panel);
bool panel_is_active(const Panel *panel);
/**
 * For button layout next to label.
 */
void panel_label_offset(const Block *block, int *r_x, int *r_y);
bool panel_should_show_background(const ARegion *region, const PanelType *panel_type);
int panel_size_y(const Panel *panel);
bool panel_is_dragging(const Panel *panel);
/**
 * Find whether a panel or any of its sub-panels contain a property that matches the search filter,
 * depending on the search process running in #block_apply_search_filter earlier.
 */
bool panel_matches_search_filter(const Panel *panel);
bool panel_can_be_pinned(const Panel *panel);

bool panel_category_is_visible(const ARegion *region);
bool panel_category_tabs_is_visible(const ARegion *region);
void panel_category_add(ARegion *region, const char *name);
PanelCategoryDyn *panel_category_find(const ARegion *region, const char *idname);
int panel_category_index_find(ARegion *region, const char *idname);
PanelCategoryStack *panel_category_active_find(ARegion *region, const char *idname);
const char *panel_category_active_get(ARegion *region, bool set_fallback);
void panel_category_active_set(ARegion *region, const char *idname);
/** \param index: index of item _in #ARegion.panels_category list_. */
void panel_category_index_active_set(ARegion *region, const int index);
void panel_category_active_set_default(ARegion *region, const char *idname);
void panel_category_clear_all(ARegion *region);
/**
 * Draw vertical tabs on the left side of the region, one tab per category.
 */
void panel_category_tabs_draw_all(ARegion *region, const char *category_id_active);

void panel_stop_animation(const bContext *C, Panel *panel);

/* Panel custom data. */
PointerRNA *panel_custom_data_get(const Panel *panel);
PointerRNA *region_panel_custom_data_under_cursor(const bContext *C, const wmEvent *event);
void panel_custom_data_set(Panel *panel, PointerRNA *custom_data);

/* Poly-instantiated panels for representing a list of data. */
/**
 * Called in situations where panels need to be added dynamically rather than
 * having only one panel corresponding to each #PanelType.
 */
Panel *panel_add_instanced(const bContext *C,
                           ARegion *region,
                           ListBaseT<Panel> *panels,
                           const char *panel_idname,
                           PointerRNA *custom_data);
/**
 * Remove instanced panels from the region's panel list.
 *
 * \note Can be called with NULL \a C, but it should be avoided because
 * handlers might not be removed.
 */
void panels_free_instanced(const bContext *C, ARegion *region);

#define INSTANCED_PANEL_UNIQUE_STR_SIZE 16
/**
 * Find a unique key to append to the #PanelType.idname for the lookup to the panel's #Block.
 * Needed for instanced panels, where there can be multiple with the same type and identifier.
 */
void list_panel_unique_str(Panel *panel, char *r_name);

using ListPanelIDFromDataFunc = void (*)(void *data_link, char *r_idname);
/**
 * Check if the instanced panels in the region's panels correspond to the list of data the panels
 * represent. Returns false if the panels have been reordered or if the types from the list data
 * don't match in any way.
 *
 * \param data: The list of data to check against the instanced panels.
 * \param panel_idname_func: Function to find the #PanelType.idname for each item in the data list.
 * For a readability and generality, this lookup happens separately for each type of panel list.
 */
bool panel_list_matches_data(ARegion *region,
                             ListBase *data,
                             ListPanelIDFromDataFunc panel_idname_func);

/* Handlers
 *
 * Handlers that can be registered in regions, areas and windows for
 * handling WM events. Mostly this is done automatic by modules such
 * as screen/ if ED_KEYMAP_UI is set, or internally in popup functions. */

void region_handlers_add(ListBaseT<wmEventHandler> *handlers);
void popup_handlers_add(bContext *C,
                        ListBaseT<wmEventHandler> *handlers,
                        PopupBlockHandle *popup,
                        char flag);
void popup_handlers_remove(ListBaseT<wmEventHandler> *handlers, PopupBlockHandle *popup);
void popup_handlers_remove_all(bContext *C, ListBaseT<wmEventHandler> *handlers);

/* Module
 *
 * init and exit should be called before using this module. init_userdef must
 * be used to reinitialize some internal state if user preferences change. */

void init();
/* after reading userdef file */
void init_userdef();
void reinit_font();
void ui_exit();

/* When changing UI font, update text style weights with default font weight
 * if non-variable. Therefore fixed weight bold font will look bold. */
void update_text_styles();

#define UI_UNIT_X ((void)0, U.widget_unit)
#define UI_UNIT_Y ((void)0, U.widget_unit)

#define UI_HEADER_OFFSET \
  ((void)0, ((U.uiflag & USER_AREA_CORNER_HANDLE) ? 16.0f : 8.0f) * UI_SCALE_FAC)

#define UI_AZONESPOTW_LEFT UI_HEADER_OFFSET       /* Width of left-side corner #AZone. */
#define UI_AZONESPOTW_RIGHT (8.0f * UI_SCALE_FAC) /* Width of right-side corner #AZone. */
#define UI_AZONESPOTH (0.6f * U.widget_unit)      /* Height of corner action zone #AZone. */

/* uiLayoutOperatorButs flags */
enum {
  TEMPLATE_OP_PROPS_SHOW_TITLE = 1 << 0,
  TEMPLATE_OP_PROPS_SHOW_EMPTY = 1 << 1,
  TEMPLATE_OP_PROPS_COMPACT = 1 << 2,
  TEMPLATE_OP_PROPS_HIDE_ADVANCED = 1 << 3,
  /* Disable property split for the default layout (custom ui callbacks still have full control
   * over the layout and can enable it). */
  TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT = 1 << 4,
  TEMPLATE_OP_PROPS_HIDE_PRESETS = 1 << 5,
  /**
   * Allow the buttons placed by the template to send an undo push. Usually this isn't wanted,
   * except for rare cases where operators draw their properties into a regular UI for later
   * execution (e.g. collection exporter panels in Properties).
   *
   * This should never be enabled for UIs that trigger redo, like "Adjust Last Operation" panels.
   */
  TEMPLATE_OP_PROPS_ALLOW_UNDO_PUSH = 1 << 6,
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
  CNR_TOP_LEFT = 1 << 0,
  CNR_TOP_RIGHT = 1 << 1,
  CNR_BOTTOM_RIGHT = 1 << 2,
  CNR_BOTTOM_LEFT = 1 << 3,
  /* just for convenience */
  CNR_NONE = 0,
  CNR_ALL = (CNR_TOP_LEFT | CNR_TOP_RIGHT | CNR_BOTTOM_RIGHT | CNR_BOTTOM_LEFT),
};

void region_message_subscribe(ARegion *region, wmMsgBus *mbus);

/**
 * This is a bit of a hack but best keep it in one place at least.
 */
wmOperatorType *button_operatortype_get_from_enum_menu(Button *but, PropertyRNA **r_prop);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
MenuType *button_menutype_get(const Button *but);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
PanelType *button_paneltype_get(const Button *but);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
std::optional<StringRefNull> button_asset_shelf_type_idname_get(const Button *but);

/* templates */
void template_header(Layout *layout, bContext *C);
void template_id(Layout *layout,
                 const bContext *C,
                 PointerRNA *ptr,
                 StringRefNull propname,
                 const char *newop,
                 const char *openop,
                 const char *unlinkop,
                 int filter = TEMPLATE_ID_FILTER_ALL,
                 bool live_icon = false,
                 std::optional<StringRef> text = std::nullopt);
void template_ID_session_uid(
    Layout &layout, bContext *C, PointerRNA *ptr, StringRefNull propname, short idcode);
void template_id_browse(Layout *layout,
                        bContext *C,
                        PointerRNA *ptr,
                        StringRefNull propname,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        int filter = TEMPLATE_ID_FILTER_ALL,
                        const char *text = nullptr);
void template_id_preview(Layout *layout,
                         bContext *C,
                         PointerRNA *ptr,
                         StringRefNull propname,
                         const char *newop,
                         const char *openop,
                         const char *unlinkop,
                         int rows,
                         int cols,
                         int filter = TEMPLATE_ID_FILTER_ALL,
                         bool hide_buttons = false);
void template_matrix(Layout *layout, PointerRNA *ptr, StringRefNull propname);
/**
 * Version of #template_id using tabs.
 */
void template_id_tabs(Layout *layout,
                      bContext *C,
                      PointerRNA *ptr,
                      StringRefNull propname,
                      const char *newop,
                      const char *menu,
                      int filter = TEMPLATE_ID_FILTER_ALL);
/**
 * This is for selecting the type of ID-block to use,
 * and then from the relevant type choosing the block to use.
 *
 * \param propname: property identifier for property that ID-pointer gets stored to.
 * \param proptypename: property identifier for property
 * used to determine the type of ID-pointer that can be used.
 */
void template_any_id(Layout *layout,
                     PointerRNA *ptr,
                     StringRefNull propname,
                     StringRefNull proptypename,
                     std::optional<StringRef> text);

/**
 * Action selector.
 *
 * This is a specialization of #template_id, hard-coded to assign Actions to the given ID.
 * Such a specialization is necessary, as the RNA property (`id.animation_data.action`) does not
 * exist when the ID's `adt` pointer is `nullptr`. In that case template_id will not be able
 * to find the RNA type of that property, which in turn it needs to determine the type of IDs to
 * show.
 */
void template_action(Layout *layout,
                     const bContext *C,
                     ID *id,
                     const char *newop,
                     const char *unlinkop,
                     std::optional<StringRef> text);

/**
 * Search menu to pick an item from a collection.
 * A version of template_id that works for non-ID types.
 */
void template_search(Layout *layout,
                     const bContext *C,
                     PointerRNA *ptr,
                     StringRefNull propname,
                     PointerRNA *searchptr,
                     const char *searchpropname,
                     const char *newop,
                     const char *unlinkop,
                     std::optional<StringRef> text = std::nullopt);
void template_search_preview(Layout *layout,
                             bContext *C,
                             PointerRNA *ptr,
                             StringRefNull propname,
                             PointerRNA *searchptr,
                             const char *searchpropname,
                             const char *newop,
                             const char *unlinkop,
                             int rows,
                             int cols,
                             std::optional<StringRef> text = std::nullopt);
/**
 * This is creating/editing RNA-Paths
 *
 * - ptr: struct which holds the path property
 * - propname: property identifier for property that path gets stored to
 * - root_ptr: struct that path gets built from
 */
void template_path_builder(Layout *layout,
                           PointerRNA *ptr,
                           StringRefNull propname,
                           PointerRNA *root_ptr,
                           std::optional<StringRefNull> text);
void template_modifiers(Layout *layout, bContext *C);
void template_strip_modifiers(Layout *layout, bContext *C);
/**
 * Check if the shader effect panels don't match the data and rebuild the panels if so.
 */
void template_shader_fx(Layout *layout, bContext *C);
/**
 * Check if the constraint panels don't match the data and rebuild the panels if so.
 */
void template_constraints(Layout *layout, bContext *C, bool use_bone_constraints);

void template_greasepencil_color_preview(Layout *layout,
                                         bContext *C,
                                         PointerRNA *ptr,
                                         StringRefNull propname,
                                         int rows,
                                         int cols,
                                         float scale,
                                         int filter);

void template_operator_redo_properties(Layout *layout, const bContext *C);

void template_constraint_header(Layout *layout, PointerRNA *ptr);
void template_preview(Layout *layout,
                      bContext *C,
                      ID *id,
                      bool show_buttons,
                      ID *parent,
                      MTex *slot,
                      const char *preview_id);
void template_color_ramp(Layout *layout, PointerRNA *ptr, StringRefNull propname, bool expand);
/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
void template_icon(Layout *layout, int icon_value, float icon_scale);
/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
void template_icon_view(Layout *layout,
                        PointerRNA *ptr,
                        StringRefNull propname,
                        bool show_labels,
                        float icon_scale,
                        float icon_scale_popup);
void template_histogram(Layout *layout, PointerRNA *ptr, StringRefNull propname);
void template_waveform(Layout *layout, PointerRNA *ptr, StringRefNull propname);
void template_vectorscope(Layout *layout, PointerRNA *ptr, StringRefNull propname);
void template_curve_mapping(Layout *layout,
                            PointerRNA *ptr,
                            StringRefNull propname,
                            int type,
                            bool levels,
                            bool brush,
                            bool neg_slope,
                            bool tone,
                            bool presets);
/**
 * Template for a path creation widget intended for custom bevel profiles.
 * This section is quite similar to #template_curve_mapping, but with reduced complexity.
 */
void template_curve_profile(Layout *layout, PointerRNA *ptr, StringRefNull propname);
/**
 * This template now follows User Preference for type - name is not correct anymore.
 */
void template_color_picker(Layout *layout,
                           PointerRNA *ptr,
                           StringRefNull propname,
                           bool value_slider,
                           bool lock,
                           bool lock_luminosity,
                           bool cubic);
void template_palette(Layout *layout, PointerRNA *ptr, StringRefNull propname, bool colors);
void template_crypto_picker(Layout *layout, PointerRNA *ptr, StringRefNull propname, int icon);
/**
 * TODO: for now, grouping of layers is determined by dividing up the length of
 * the array of layer bit-flags.
 */
void template_layers(Layout *layout,
                     PointerRNA *ptr,
                     StringRefNull propname,
                     PointerRNA *used_ptr,
                     const char *used_propname,
                     int active_layer);

}  // namespace ui

void uiTemplateImage(ui::Layout *layout,
                     bContext *C,
                     PointerRNA *ptr,
                     StringRefNull propname,
                     PointerRNA *userptr,
                     bool compact,
                     bool multiview);
void uiTemplateImageSettings(ui::Layout *layout,
                             bContext *C,
                             PointerRNA *imfptr,
                             bool color_management,
                             const char *panel_idname = nullptr);
void uiTemplateImageStereo3d(ui::Layout *layout, PointerRNA *stereo3d_format_ptr);
void uiTemplateImageViews(ui::Layout *layout, PointerRNA *imaptr);
void uiTemplateImageFormatViews(ui::Layout *layout, PointerRNA *imfptr, PointerRNA *ptr);
void uiTemplateImageLayers(ui::Layout *layout, bContext *C, Image *ima, ImageUser *iuser);
void uiTemplateImageInfo(ui::Layout *layout, bContext *C, Image *ima, ImageUser *iuser);

namespace ui {
void template_running_jobs(Layout *layout, bContext *C);
void button_func_operator_search(Button *but);
void uiTemplateOperatorSearch(Layout *layout);

void button_func_menu_search(Button *but, const char *single_menu_idname = nullptr);
void uiTemplateMenuSearch(Layout *layout);

/**
 * Draw Operator property buttons for redoing execution with different settings.
 * This function does not initialize the layout,
 * functions can be called on the layout before and after.
 */
void uiTemplateOperatorPropertyButs(
    const bContext *C, Layout *layout, wmOperator *op, eButLabelAlign label_align, short flag);
}  // namespace ui
void template_header3D_mode(ui::Layout *layout, bContext *C);
void uiTemplateEditModeSelection(ui::Layout *layout, bContext *C);
namespace ui {
void uiTemplateReportsBanner(Layout *layout, bContext *C);
void uiTemplateInputStatus(Layout *layout, bContext *C);
void uiTemplateStatusInfo(Layout *layout, bContext *C);
void uiTemplateKeymapItemProperties(Layout *layout, PointerRNA *ptr);

bool template_event_from_keymap_item(Layout *layout,
                                     StringRefNull text,
                                     const wmKeyMapItem *kmi,
                                     bool text_fallback);

/* Draw keymap item for status bar. Returns number of items consumed,
 * as X/Y/Z items may get merged to use less space. */
int template_status_bar_modal_item(Layout *layout,
                                   wmOperator *op,
                                   const wmKeyMap *keymap,
                                   const EnumPropertyItem *item);

void template_component_menu(Layout *layout,
                             PointerRNA *ptr,
                             StringRefNull propname,
                             StringRef name);
void template_node_socket(Layout *layout, bContext *C, const float color[4]);

/**
 * Draw the main CacheFile properties and operators (file path, scale, etc.), that is those which
 * do not have their own dedicated template functions.
 */
void template_cache_file(Layout *layout,
                         const bContext *C,
                         PointerRNA *ptr,
                         StringRefNull propname);

/**
 * Lookup the CacheFile PointerRNA of the given pointer and return it in the output parameter.
 * Returns true if `ptr` has a RNACacheFile, false otherwise. If false, the output parameter is not
 * initialized.
 */
bool template_cache_file_pointer(PointerRNA *ptr, StringRefNull propname, PointerRNA *r_file_ptr);

/**
 * Draw the velocity related properties of the CacheFile.
 */
void template_cache_file_velocity(Layout *layout, PointerRNA *fileptr);

/**
 * Draw the time related properties of the CacheFile.
 */
void template_cache_file_time_settings(Layout *layout, PointerRNA *fileptr);

/**
 * Draw the override layers related properties of the CacheFile.
 */
void template_list_flags(Layout *layout, const bContext *C, PointerRNA *fileptr);

/** Default UIList class name, keep in sync with its declaration in `bl_ui/__init__.py`. */
#define UI_UL_DEFAULT_CLASS_NAME "UI_UL_list"
enum TemplateListFlags {
  TEMPLATE_LIST_FLAG_NONE = 0,
  TEMPLATE_LIST_SORT_REVERSE = (1 << 0),
  TEMPLATE_LIST_SORT_LOCK = (1 << 1),
  /** Don't allow resizing the list, i.e. don't add the grip button. */
  TEMPLATE_LIST_NO_GRIP = (1 << 2),
};
ENUM_OPERATORS(TemplateListFlags);

void template_list(Layout *layout,
                   const bContext *C,
                   const char *listtype_name,
                   const char *list_id,
                   PointerRNA *dataptr,
                   StringRefNull propname,
                   PointerRNA *active_dataptr,
                   StringRefNull active_propname,
                   const char *item_dyntip_propname,
                   int rows,
                   int maxrows,
                   int layout_type,
                   enum TemplateListFlags flags);
}  // namespace ui

void uiTemplateNodeLink(
    ui::Layout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input);
void uiTemplateNodeView(
    ui::Layout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input);

void uiTemplateTextureUser(ui::Layout *layout, bContext *C);

/**
 * Button to quickly show texture in Properties Editor texture tab.
 */
void uiTemplateTextureShow(ui::Layout *layout,
                           const bContext *C,
                           PointerRNA *ptr,
                           PropertyRNA *prop);

void uiTemplateMovieClip(
    ui::Layout *layout, bContext *C, PointerRNA *ptr, StringRefNull propname, bool compact);
void uiTemplateTrack(ui::Layout *layout, PointerRNA *ptr, StringRefNull propname);
void uiTemplateMarker(ui::Layout *layout,
                      PointerRNA *ptr,
                      StringRefNull propname,
                      PointerRNA *userptr,
                      PointerRNA *trackptr,
                      bool compact);
void uiTemplateMovieclipInformation(ui::Layout *layout,
                                    PointerRNA *ptr,
                                    StringRefNull propname,
                                    PointerRNA *userptr);
namespace ui {

void template_colorspace_settings(Layout *layout, PointerRNA *ptr, StringRefNull propname);
void template_colormanaged_view_settings(Layout *layout,
                                         bContext *C,
                                         PointerRNA *ptr,
                                         StringRefNull propname);

int template_recent_files(Layout *layout, int rows);
void template_file_select_path(Layout *layout, bContext *C, FileSelectParams *params);

void template_asset_shelf_popover(
    Layout &layout, const bContext &C, StringRefNull asset_shelf_id, StringRef name, int icon);

void template_light_linking_collection(
    Layout *layout, bContext *C, Layout *context_layout, PointerRNA *ptr, StringRefNull propname);

void template_bone_collection_tree(Layout *layout, bContext *C);
void template_grease_pencil_layer_tree(Layout *layout, bContext *C);

void template_tree_interface(Layout *layout, const bContext *C, PointerRNA *ptr);
/**
 * Draw all node buttons and socket default values with the same panel structure used by the node.
 */
void template_node_inputs(Layout *layout, bContext *C, PointerRNA *ptr);

void template_collection_exporters(Layout *layout, bContext *C);

}  // namespace ui

namespace ed::object::shapekey {
void template_tree(ui::Layout *layout, bContext *C);
}

namespace ui {
/**
 * \return: True if the list item with unfiltered, unordered index \a item_idx is visible given the
 *          current filter settings.
 */
bool list_item_index_is_filtered_visible(const struct uiList *ui_list, int item_idx);

/* UI Operators */
struct DragColorHandle {
  float color[4];
  bool gamma_corrected;
  bool has_alpha;
};

void operatortypes_ui();
/**
 * \brief User Interface Keymap
 */
void keymap_ui(wmKeyConfig *keyconf);
void dropboxes_ui();
void uilisttypes_ui();

void drop_color_copy(bContext *C, wmDrag *drag, wmDropBox *drop);
bool drop_color_poll(bContext *C, wmDrag *drag, const wmEvent *event);

bool context_copy_to_selected_list(bContext *C,
                                   PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   Vector<PointerRNA> *r_lb,
                                   bool *r_use_path_from_id,
                                   std::optional<std::string> *r_path);
bool context_copy_to_selected_check(PointerRNA *ptr,
                                    PointerRNA *ptr_link,
                                    PropertyRNA *prop,
                                    const char *path,
                                    bool use_path_from_id,
                                    PointerRNA *r_ptr,
                                    PropertyRNA **r_prop);

/* Helpers for Operators */
Button *context_active_but_get(const bContext *C);
/**
 * Version of #UI_context_active_get() that uses the result of #CTX_wm_region_popup() if set.
 * Does not traverse into parent menus, which may be wanted in some cases.
 */
Button *context_active_but_get_respect_popup(const bContext *C);
/**
 * Version of #context_active_but_get that also returns RNA property info.
 * Helper function for insert keyframe, reset to default, etc operators.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
Button *context_active_but_prop_get(const bContext *C,
                                    PointerRNA *r_ptr,
                                    PropertyRNA **r_prop,
                                    int *r_index);

/**
 * As above, but for a specified region.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
Button *region_active_but_prop_get(const ARegion *region,
                                   PointerRNA *r_ptr,
                                   PropertyRNA **r_prop,
                                   int *r_index);

void context_active_but_prop_handle(bContext *C, bool handle_undo);
void context_active_but_clear(bContext *C, wmWindow *win, ARegion *region);

wmOperator *context_active_operator_get(const bContext *C);
/**
 * Helper function for insert keyframe, reset to default, etc operators.
 */
void context_update_anim_flag(const bContext *C);
void context_active_but_prop_get_filebrowser(const bContext *C,
                                             PointerRNA *r_ptr,
                                             PropertyRNA **r_prop,
                                             bool *r_is_undo,
                                             bool *r_is_userdef);
/**
 * For new/open operators.
 *
 * This is for browsing and editing the ID-blocks used.
 */
void context_active_but_prop_get_templateID(const bContext *C,
                                            PointerRNA *r_ptr,
                                            PropertyRNA **r_prop);
ID *context_active_but_get_tab_ID(bContext *C);

Button *region_active_but_get(const ARegion *region);
Button *region_but_find_rect_over(const ARegion *region, const rcti *rect_px);
Block *region_block_find_mouse_over(const ARegion *region, const int xy[2], bool only_clip);
/**
 * Try to find a search-box region opened from a button in \a button_region.
 */
ARegion *region_searchbox_region_get(const ARegion *button_region);

/** #uiFontStyle.align */
enum FontStyleAlign {
  UI_STYLE_TEXT_LEFT = 0,
  UI_STYLE_TEXT_CENTER = 1,
  UI_STYLE_TEXT_RIGHT = 2,
};

struct FontStyleDrawParams {
  FontStyleAlign align;
  uint word_wrap : 1;
};

/* Styled text draw */
void fontstyle_set(const uiFontStyle *fs);
void fontstyle_draw_ex(const uiFontStyle *fs,
                       const rcti *rect,
                       const char *str,
                       size_t str_len,
                       const uchar col[4],
                       const FontStyleDrawParams *fs_params,
                       int *r_xofs,
                       int *r_yofs,
                       ResultBLF *r_info);

void fontstyle_draw(const uiFontStyle *fs,
                    const rcti *rect,
                    const char *str,
                    size_t str_len,
                    const uchar col[4],
                    const FontStyleDrawParams *fs_params);

void fontstyle_draw_multiline_clipped_ex(const uiFontStyle *fs,
                                         const rcti *rect,
                                         const char *str,
                                         const uchar col[4],
                                         FontStyleAlign align,
                                         int *r_xofs,
                                         int *r_yofs,
                                         ResultBLF *r_info);
/**
 * Draws text with wrapping and shortening using "..." so that it fits into the given rectangle.
 */
void fontstyle_draw_multiline_clipped(const uiFontStyle *fs,
                                      const rcti *rect,
                                      const char *str,
                                      const uchar col[4],
                                      FontStyleAlign align);

/**
 * Drawn same as #fontstyle_draw, but at 90 degree angle.
 */
void fontstyle_draw_rotated(const uiFontStyle *fs,
                            const rcti *rect,
                            const char *str,
                            const uchar col[4]);
/**
 * Similar to #fontstyle_draw
 * but ignore alignment, shadow & no clipping rect.
 *
 * For drawing on-screen labels.
 */
void fontstyle_draw_simple(
    const uiFontStyle *fs, float x, float y, const char *str, const uchar col[4]);
/**
 * Same as #fontstyle_draw but draw a colored backdrop.
 */
void fontstyle_draw_simple_backdrop(const uiFontStyle *fs,
                                    float x,
                                    float y,
                                    StringRef str,
                                    const float col_fg[4],
                                    const float col_bg[4]);

int fontstyle_string_width(const uiFontStyle *fs, const char *str) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
/**
 * Return the width of `str` with the spacing & kerning of `fs` with `aspect`
 * (representing #Block.aspect) applied.
 *
 * When calculating text width, the UI layout logic calculate widths without scale,
 * only applying scale when drawing. This causes problems for fonts since kerning at
 * smaller sizes often makes them wider than a scaled down version of the larger text.
 * Resolve this by calculating the text at the on-screen size,
 * returning the result scaled back to 1:1. See #92361.
 */
int fontstyle_string_width_with_block_aspect(const uiFontStyle *fs,
                                             StringRef str,
                                             float aspect) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int fontstyle_height_max(const uiFontStyle *fs);

/**
 * Triangle 'icon' for panel header and other cases.
 */
void draw_icon_tri(float x, float y, char dir, const float[4]);

/**
 * Read a style (without any scaling applied).
 */
const uiStyle *style_get(); /* use for fonts etc */
/**
 * Read a style (with the current DPI applied).
 */
const uiStyle *style_get_dpi();

/* #UI_OT_editsource helpers. */
bool editsource_enable_check();
void editsource_active_but_test(Button *but);

/**
 * Adjust the view so the rectangle of \a but is in view, with some extra margin.
 *
 * It's important that this is only executed after buttons received their final #Button.rect. E.g.
 * #panels_end() modifies them, so if that is executed, this function must not be called before
 * it.
 *
 * \param region: The region the button is placed in. Make sure this is actually the one the button
 *                is placed in, not just the context region.
 */
void but_ensure_in_view(const bContext *C, ARegion *region, const Button *but);

/* UI_butstore_ helpers */
struct ButStore;

/**
 * Create a new button store, the caller must manage and run #butstore_free
 */
ButStore *butstore_create(Block *block);
/**
 * NULL all pointers, don't free since the owner needs to be able to inspect.
 */
void butstore_clear(Block *block);
/**
 * Map freed buttons from the old block and update pointers.
 */
void butstore_update(Block *block);
void butstore_free(Block *block, ButStore *bs_handle);
bool butstore_is_valid(ButStore *bs_handle);
bool butstore_is_registered(Block *block, Button *but);
void butstore_register(ButStore *bs_handle, Button **but_p);
/**
 * Update the pointer for a registered button.
 */
bool butstore_register_update(Block *block, Button *but_dst, const Button *but_src);
void butstore_unregister(ButStore *bs_handle, Button **but_p);

/* ui_interface_region_tooltip.c */

/**
 * \param is_quick_tip: See #button_func_quick_tooltip_set for what a quick tooltip is.
 */
ARegion *tooltip_create_from_button(bContext *C,
                                    ARegion *butregion,
                                    Button *but,
                                    bool is_quick_tip);
ARegion *tooltip_create_from_button_or_extra_icon(bContext *C,
                                                  ARegion *butregion,
                                                  Button *but,
                                                  ButtonExtraOpIcon *extra_icon,
                                                  bool is_quick_tip);
ARegion *tooltip_create_from_gizmo(bContext *C, wmGizmo *gz);
void tooltip_free(bContext *C, bScreen *screen, ARegion *region);

/**
 * Create a tooltip from search-item tooltip data \a item_tooltip data.
 * To be called from a callback set with #button_func_search_set_tooltip().
 *
 * \param item_rect: Rectangle of the search item in search region space (#searchbox_butrect())
 *                   which is passed to the tooltip callback.
 */
ARegion *tooltip_create_from_search_item_generic(bContext *C,
                                                 const ARegion *searchbox_region,
                                                 const rcti *item_rect,
                                                 ID *id);

/* How long before a tool-tip shows. */
#define UI_TOOLTIP_DELAY 0.5
#define UI_TOOLTIP_DELAY_QUICK 0.2

/* Float precision helpers */

/* Maximum number of digits of precision (not number of decimal places)
 * to display for float values. Note that the UI_FLOAT_VALUE_DISPLAY_*
 * defines that follow depend on this. */
#define UI_PRECISION_FLOAT_MAX 6

/* Values exceeding this range are displayed as "inf" / "-inf".
 * This range is almost FLT_MAX to -FLT_MAX, but each is truncated
 * to our display precision, set by UI_PRECISION_FLOAT_MAX. Each
 * is approximately `FLT_MAX / 1.000001` but that calculation does
 * not give us the explicit zeros needed for this exact range. */
#define UI_FLOAT_VALUE_DISPLAY_MAX 3.402820000e+38F
#define UI_FLOAT_VALUE_DISPLAY_MIN -3.402820000e+38F

/* For float buttons the 'step', is scaled */
#define UI_PRECISION_FLOAT_SCALE 0.01f

/* Typical UI text */
#define UI_FSTYLE_WIDGET (const uiFontStyle *)&(ui::style_get()->widget)
#define UI_FSTYLE_TOOLTIP (const uiFontStyle *)&(ui::style_get()->tooltip)

/**
 * Returns the best "UI" precision for given floating value,
 * so that e.g. 10.000001 rather gets drawn as '10'...
 */
int calc_float_precision(int prec, double value);

/* widget batched drawing */
void widgetbase_draw_cache_begin();
void widgetbase_draw_cache_flush();
void widgetbase_draw_cache_end();

/* Use for resetting the theme. */
namespace theme {

/**
 * Initialize default theme.
 *
 * \note When you add new colors, created & saved themes need initialized
 * use function below, #init_userdef_do_versions.
 */
void init_default();

}  // namespace theme

void style_init_default();

void interface_tag_script_reload();

/** Special drawing for toolbar, mainly workarounds for inflexible icon sizing. */
#define USE_UI_TOOLBAR_HACK

/** Support click-drag motion which presses the button and closes a popover (like a menu). */
#define USE_UI_POPOVER_ONCE

bool view_item_matches(const AbstractViewItem &a, const AbstractViewItem &b);
/**
 * Can \a item be renamed right now? Note that this isn't just a mere wrapper around
 * #AbstractViewItem::supports_renaming(). This also checks if there is another item being renamed,
 * and returns false if so.
 */
bool view_item_can_rename(const AbstractViewItem &item);
void view_item_begin_rename(AbstractViewItem &item);

bool view_item_supports_drag(const AbstractViewItem &item);
/** If this view is displayed in a popup, don't close it when clicking to activate items. */
bool view_item_popup_keep_open(const AbstractViewItem &item);
/**
 * Attempt to start dragging \a item_. This will not work if the view item doesn't
 * support dragging, i.e. if it won't create a drag-controller upon request.
 * \return True if dragging started successfully, otherwise false.
 */
bool view_item_drag_start(bContext &C, AbstractViewItem &item);

/**
 * \param xy: Coordinate to find a view item at, in window space.
 * \param pad: Extra padding added to the bounding box of the view.
 */
AbstractView *region_view_find_at(const ARegion *region, const int xy[2], int pad);
/**
 * \param xy: Coordinate to find a view item at, in window space.
 */
AbstractViewItem *region_views_find_item_at(const ARegion &region, const int xy[2]);
AbstractViewItem *region_views_find_active_item(const ARegion *region);
Button *region_views_find_active_item_but(const ARegion *region);
void region_views_clear_search_highlight(const ARegion *region);

}  // namespace ui
}  // namespace blender
