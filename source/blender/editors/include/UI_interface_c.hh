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

#include "DNA_userdef_types.h"

#include "UI_interface_icons.hh"
#include "UI_interface_types.hh"

#include "WM_types.hh"

#include "MEM_guardedalloc.h"

/* Struct Declarations */

struct ARegion;
struct AutoComplete;
struct Depsgraph;
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
namespace blender::ed::asset {
struct AssetFilterSettings;
}
namespace blender::ui {
class AbstractView;
class AbstractViewItem;
}  // namespace blender::ui

struct uiBlock;
struct uiBut;
struct uiButExtraOpIcon;
struct uiLayout;
struct uiPopupBlockHandle;
struct uiTooltipData;

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

/**
 * For #ARegion.overlap regions, pass events though if they don't overlap
 * the regions contents (the usable part of the #View2D and buttons).
 *
 * The margin is needed so it's not possible to accidentally click in between buttons.
 */
#define UI_REGION_OVERLAP_MARGIN (U.widget_unit / 3)

/** Use for clamping popups within the screen. */
#define UI_SCREEN_MARGIN 10

namespace blender::ui {
/** #uiBlock.emboss and #uiBut.emboss */
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

}  // namespace blender::ui

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
  UI_BLOCK_NUMSELECT = 1 << 1,
  /** Don't apply window clipping. */
  UI_BLOCK_NO_WIN_CLIP = 1 << 2,
  UI_BLOCK_CLIPBOTTOM = 1 << 3,
  UI_BLOCK_CLIPTOP = 1 << 4,
  UI_BLOCK_MOVEMOUSE_QUIT = 1 << 5,
  UI_BLOCK_KEEP_OPEN = 1 << 6,
  UI_BLOCK_POPUP = 1 << 7,
  UI_BLOCK_OUT_1 = 1 << 8,
  UI_BLOCK_SEARCH_MENU = 1 << 9,
  UI_BLOCK_POPUP_MEMORY = 1 << 10,
  /** Stop handling mouse events. */
  UI_BLOCK_CLIP_EVENTS = 1 << 11,

  /* #uiBlock::flags bits 14-17 are identical to #uiBut::drawflag bits. */

  UI_BLOCK_POPUP_HOLD = 1 << 18,
  UI_BLOCK_LIST_ITEM = 1 << 19,
  UI_BLOCK_PIE_MENU = 1 << 20,
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
  /** Don't accelerator keys for the items in the block. */
  UI_BLOCK_NO_ACCELERATOR_KEYS = 1 << 27,
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

enum {
  /**
   * This is used when `UI_BUT_ACTIVATE_ON_INIT` is used, which is used to activate e.g. a search
   * box as soon as a popup opens. Usually, the text in the search box is selected by default.
   * However, sometimes this behavior is not desired, so it can be disabled with this flag.
   */
  UI_BUT2_ACTIVATE_ON_INIT_NO_SELECT = 1 << 0,
  /**
   * Force the button as active in a semi-modal state. For example, text buttons can continuously
   * capture text input, while leaving the remaining UI interactive. Only supported well for text
   * buttons currently.
   */
  UI_BUT2_FORCE_SEMI_MODAL_ACTIVE = 1 << 1,
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
   * See #UI_but_func_quick_tooltip_set.
   */
  UI_BUT_HAS_QUICK_TOOLTIP = 1 << 5,
  /** Do not add the usual horizontal padding for text drawing. */
  UI_BUT_NO_TEXT_PADDING = 1 << 6,
  /** Do not add the usual padding around preview image drawing, use the size of the button. */
  UI_BUT_NO_PREVIEW_PADDING = 1 << 7,

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

  /** Mouse is hovering left part of number button */
  UI_BUT_HOVER_LEFT = 1 << 21,
  /** Mouse is hovering right part of number button */
  UI_BUT_HOVER_RIGHT = 1 << 22,

  /** Reverse order of consecutive off/on icons */
  UI_BUT_ICON_REVERSE = 1 << 23,

  /** Value is animated, but the current value differs from the animated one. */
  UI_BUT_ANIMATED_CHANGED = 1 << 24,

  /** Draw the checkbox buttons inverted. */
  UI_BUT_CHECKBOX_INVERT = 1 << 25,

  /** Drawn in a way that indicates that the state/value is unknown. */
  UI_BUT_INDETERMINATE = 1 << 26,

  /** Draw icon inverted to indicate a special state. */
  UI_BUT_ICON_INVERT = 1 << 27,
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
/** \note requires `uiBut::poin != nullptr`. */
#define UI_BUT_POIN_TYPES (ButPointerType::Float | ButPointerType::Short | ButPointerType::Char)

enum class ButType : int8_t {
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

  /** Buttons with value >= #ButType::SearchMenu don't get undo pushes. */
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
namespace blender::ui {
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
}  // namespace blender::ui

struct uiButTypeWithPointerType {
  ButType but_type = ButType::But;
  /**
   * Buttons can access source data with RNA pointers or raw pointers (#uiBut::poin), when using a
   * raw pointer to numerical values this indicates the underlying type of the source data.
   */
  ButPointerType pointer_type = ButPointerType::None;
  /**
   * Indicates the bit index when the raw pointed data stores boolean bit values,
   * which is indicated with the #ButPointerType::Bit flag.
   */
  char bit_index = 0;

  uiButTypeWithPointerType(ButType bt) : but_type{bt} {}

  uiButTypeWithPointerType(ButType bt, ButPointerType pt) : but_type{bt}, pointer_type{pt} {}

  uiButTypeWithPointerType(ButType bt, ButPointerType pt, int i)
      : but_type{bt}, pointer_type{pt}, bit_index{char(i)}
  {
    BLI_assert(bool(pointer_type & ButPointerType::Bit));
    BLI_assert(bit_index >= 0);
    BLI_assert(bit_index <
               blender::ui::but_pointer_bit_max_index(pointer_type & ~ButPointerType::Bit));
  }
};

/** Gradient types, for color picker #ButType::HsvCube etc. */
enum eButGradientType {
  UI_GRAD_NONE = -1,
  UI_GRAD_SV = 0,
  UI_GRAD_HV = 1,
  UI_GRAD_HS = 2,
  UI_GRAD_H = 3,
  UI_GRAD_S = 4,
  UI_GRAD_V = 5,

  UI_GRAD_V_ALT = 9,
  UI_GRAD_L_ALT = 10,
};

/* Drawing
 *
 * Functions to draw various shapes, taking theme settings into account.
 * Used for code that draws its own UI style elements. */

void UI_draw_roundbox_corner_set(int type);
void UI_draw_roundbox_aa(const rctf *rect, bool filled, float rad, const float color[4]);
void UI_draw_roundbox_4fv(const rctf *rect, bool filled, float rad, const float col[4]);
void UI_draw_roundbox_3ub_alpha(
    const rctf *rect, bool filled, float rad, const unsigned char col[3], unsigned char alpha);
void UI_draw_roundbox_3fv_alpha(
    const rctf *rect, bool filled, float rad, const float col[3], float alpha);
void UI_draw_roundbox_4fv_ex(const rctf *rect,
                             const float inner1[4],
                             const float inner2[4],
                             float shade_dir,
                             const float outline[4],
                             float outline_width,
                             float rad);

#if 0 /* unused */
int UI_draw_roundbox_corner_get();
#endif

void ui_draw_dropshadow(const rctf *rct, float radius, float width, float aspect, float alpha);

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
                        const rctf *rect,
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
void UI_draw_widget_scroll(uiWidgetColors *wcol, const rcti *rect, const rcti *slider, int state);

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
float UI_text_clip_middle_ex(const uiFontStyle *fstyle,
                             char *str,
                             float okwidth,
                             float minwidth,
                             size_t max_len,
                             char rpart_sep,
                             bool clip_right_if_tight = true);

blender::Vector<blender::StringRef> UI_text_clip_multiline_middle(
    const uiFontStyle *fstyle,
    const char *str,
    char *clipped_str_buf,
    const size_t max_len_clipped_str_buf,
    const float max_line_width,
    const int max_lines);

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

struct uiSearchItems;

using uiButHandleFunc = void (*)(bContext *C, void *arg1, void *arg2);
using uiButHandleRenameFunc = void (*)(bContext *C, void *arg, char *origstr);
using uiButHandleNFunc = void (*)(bContext *C, void *argN, void *arg2);
using uiButHandleHoldFunc = void (*)(bContext *C, ARegion *butregion, uiBut *but);
using uiButCompleteFunc = int (*)(bContext *C, char *str, void *arg);

/**
 * Signatures of callbacks used to free or copy some 'owned' void pointer data (like e.g.
 * #func_argN in #uiBut or #uiBlock).
 */
using uiButArgNFree = void (*)(void *argN);
using uiButArgNCopy = void *(*)(const void *argN);

/**
 * Function to compare the identity of two buttons over redraws, to check if they represent the
 * same data, and thus should be considered the same button over redraws.
 */
using uiButIdentityCompareFunc = bool (*)(const uiBut *a, const uiBut *b);

/* Search types. */
using uiButSearchCreateFn = ARegion *(*)(bContext * C,
                                         ARegion *butregion,
                                         uiButSearch *search_but);
/**
 * `is_first` is typically used to ignore search filtering when the menu is first opened in order
 * to display the full list of options. The value will be false after the button's text is edited
 * (for every call except the first).
 */
using uiButSearchUpdateFn =
    void (*)(const bContext *C, void *arg, const char *str, uiSearchItems *items, bool is_first);
using uiButSearchContextMenuFn = bool (*)(bContext *C,
                                          void *arg,
                                          void *active,
                                          const wmEvent *event);
using uiButSearchTooltipFn =
    ARegion *(*)(bContext * C, ARegion *region, const rcti *item_rect, void *arg, void *active);
using uiButSearchListenFn = void (*)(const wmRegionListenerParams *params, void *arg);

using uiBlockHandleFunc = void (*)(bContext *C, void *arg, int event);

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
using uiBlockInteractionBeginFn = void *(*)(bContext * C,
                                            const uiBlockInteraction_Params *params,
                                            void *arg1);
using uiBlockInteractionEndFn = void (*)(bContext *C,
                                         const uiBlockInteraction_Params *params,
                                         void *arg1,
                                         void *user_data);
using uiBlockInteractionUpdateFn = void (*)(bContext *C,
                                            const uiBlockInteraction_Params *params,
                                            void *arg1,
                                            void *user_data);

struct uiBlockInteraction_CallbackData {
  uiBlockInteractionBeginFn begin_fn;
  uiBlockInteractionEndFn end_fn;
  uiBlockInteractionUpdateFn update_fn;
  void *arg1;
};

void UI_block_interaction_set(uiBlock *block, uiBlockInteraction_CallbackData *callbacks);

/** \} */

/* `interface_query.cc` */

bool UI_but_has_quick_tooltip(const uiBut *but);
bool UI_but_is_tool(const uiBut *but);
/** File selectors are exempt from UTF8 checks. */
bool UI_but_is_utf8(const uiBut *but);
#define UI_but_is_decorator(but) ((but)->type == ButType::Decorator)

bool UI_block_is_empty_ex(const uiBlock *block, bool skip_title);
bool UI_block_is_empty(const uiBlock *block);
bool UI_block_can_add_separator(const uiBlock *block);
/**
 * Return true when the block has a default button.
 * Use this for popups to detect when pressing "Return" will run an action.
 */
bool UI_block_has_active_default_button(const uiBlock *block);

/**
 * Find a button under the mouse cursor, ignoring non-interactive ones (like labels). Holding Ctrl
 * over a label button that can be Ctrl-Clicked to turn into an edit button will return that.
 * Labels that are only interactive for the sake of displaying a tooltip are ignored too.
 */
uiBut *UI_but_find_mouse_over(const ARegion *region, const wmEvent *event) ATTR_WARN_UNUSED_RESULT;

uiList *UI_list_find_mouse_over(const ARegion *region, const wmEvent *event);

/* `interface_region_menu_popup.cc` */

/**
 * Popup Menus
 *
 * Functions used to create popup menus. For more extended menus the
 * UI_popup_menu_begin/End functions can be used to define own items with
 * the uiItem functions in between. If it is a simple confirmation menu
 * or similar, popups can be created with a single function call.
 */
struct uiPopupMenu;

uiPopupMenu *UI_popup_menu_begin(bContext *C, const char *title, int icon) ATTR_NONNULL();
/**
 * Directly create a popup menu that is not refreshed on redraw.
 *
 * Only return handler, and set optional title.
 * \param block_name: Assigned to uiBlock.name (useful info for debugging).
 */
uiPopupMenu *UI_popup_menu_begin_ex(bContext *C,
                                    const char *title,
                                    const char *block_name,
                                    int icon) ATTR_NONNULL();
/**
 * Set the whole structure to work.
 */
void UI_popup_menu_end(bContext *C, uiPopupMenu *pup);
bool UI_popup_menu_end_or_cancel(bContext *C, uiPopupMenu *pup);
uiLayout *UI_popup_menu_layout(uiPopupMenu *pup);

void UI_popup_menu_reports(bContext *C, ReportList *reports) ATTR_NONNULL();
wmOperatorStatus UI_popup_menu_invoke(bContext *C, const char *idname, ReportList *reports)
    ATTR_NONNULL(1, 2);

/**
 * If \a block is displayed in a popup menu, tag it for closing.
 * \param is_cancel: If set to true, the popup will be closed as being cancelled (e.g. when
 *                   pressing escape) as opposed to being handled successfully.
 */
void UI_popup_menu_close(const uiBlock *block, bool is_cancel = false);
/**
 * Version of #UI_popup_menu_close() that can be called on a button contained in a popup menu
 * block. Convenience since the block may not be available.
 */
void UI_popup_menu_close_from_but(const uiBut *but, bool is_cancel = false);

/**
 * Allow setting menu return value from externals.
 * E.g. WM might need to do this for exiting files correctly.
 */
void UI_popup_menu_retval_set(const uiBlock *block, int retval, bool enable);
/**
 * Set a dummy panel in the popup `block` to support using layout panels, the panel is linked
 * to the popup `region` so layout panels state can be persistent until the popup is closed.
 */
void UI_popup_dummy_panel_set(ARegion *region, uiBlock *block);
/**
 * Setting the button makes the popup open from the button instead of the cursor.
 */
void UI_popup_menu_but_set(uiPopupMenu *pup, ARegion *butregion, uiBut *but);

/* `interface_region_popover.cc` */

struct uiPopover;

wmOperatorStatus UI_popover_panel_invoke(bContext *C,
                                         const char *idname,
                                         bool keep_open,
                                         ReportList *reports);

/**
 * Only return handler, and set optional title.
 *
 * \param from_active_button: Use the active button for positioning,
 * use when the popover is activated from an operator instead of directly from the button.
 */
uiPopover *UI_popover_begin(bContext *C, int ui_menu_width, bool from_active_button)
    ATTR_NONNULL(1);
/**
 * Set the whole structure to work.
 */
void UI_popover_end(bContext *C, uiPopover *pup, wmKeyMap *keymap);
uiLayout *UI_popover_layout(uiPopover *pup);
void UI_popover_once_clear(uiPopover *pup);

/* `interface_region_menu_pie.cc` */

/* Pie menus */
struct uiPieMenu;

wmOperatorStatus UI_pie_menu_invoke(bContext *C, const char *idname, const wmEvent *event);

uiPieMenu *UI_pie_menu_begin(bContext *C, const char *title, int icon, const wmEvent *event)
    ATTR_NONNULL();
void UI_pie_menu_end(bContext *C, uiPieMenu *pie);
uiLayout *UI_pie_menu_layout(uiPieMenu *pie);

/* `interface_region_menu_popup.cc` */

/* Popup Blocks
 *
 * Functions used to create popup blocks. These are like popup menus
 * but allow using all button types and creating their own layout. */
using uiBlockCreateFunc = uiBlock *(*)(bContext * C, ARegion *region, void *arg1);
using uiBlockCancelFunc = void (*)(bContext *C, void *arg1);

void UI_popup_block_invoke(bContext *C, uiBlockCreateFunc func, void *arg, uiFreeArgFunc arg_free);
/**
 * \param can_refresh: When true, the popup may be refreshed (updated after creation).
 * \note It can be useful to disable refresh (even though it will work)
 * as this exits text fields which can be disruptive if refresh isn't needed.
 */
void UI_popup_block_invoke_ex(
    bContext *C, uiBlockCreateFunc func, void *arg, uiFreeArgFunc arg_free, bool can_refresh);
void UI_popup_block_ex(bContext *C,
                       uiBlockCreateFunc func,
                       uiBlockHandleFunc popup_func,
                       uiBlockCancelFunc cancel_func,
                       void *arg,
                       wmOperator *op);

/**
 * Return true when #UI_popup_block_template_confirm and related functions are supported.
 */
bool UI_popup_block_template_confirm_is_supported(const uiBlock *block);
/**
 * Create confirm & cancel buttons in a popup using callback functions.
 */
void UI_popup_block_template_confirm(uiBlock *block,
                                     bool cancel_default,
                                     blender::FunctionRef<uiBut *()> confirm_fn,
                                     blender::FunctionRef<uiBut *()> cancel_fn);
/**
 * Create confirm & cancel buttons in a popup using an operator.
 *
 * \param confirm_text: The text to confirm, null for default text or an empty string to hide.
 * \param cancel_text: The text to cancel, null for default text or an empty string to hide.
 * \param r_ptr: The pointer for operator properties, set a "confirm" button has been created.
 */
void UI_popup_block_template_confirm_op(uiLayout *layout,
                                        wmOperatorType *ot,
                                        std::optional<blender::StringRef> confirm_text,
                                        std::optional<blender::StringRef> cancel_text,
                                        const int icon,
                                        bool cancel_default,
                                        PointerRNA *r_ptr);

#if 0 /* UNUSED */
void uiPupBlockOperator(bContext *C,
                        uiBlockCreateFunc func,
                        wmOperator *op,
                        blender::wm::OpCallContext opcontext);
#endif

void UI_popup_block_close(bContext *C, wmWindow *win, uiBlock *block);

bool UI_popup_block_name_exists(const bScreen *screen, blender::StringRef name);

/* Blocks
 *
 * Functions for creating, drawing and freeing blocks. A Block is a
 * container of buttons and used for various purposes.
 *
 * Begin/Define Buttons/End/Draw is the typical order in which these
 * function should be called, though for popup blocks Draw is left out.
 * Freeing blocks is done by the screen/ module automatically.
 */

uiBlock *UI_block_begin(const bContext *C,
                        ARegion *region,
                        std::string name,
                        blender::ui::EmbossType emboss);
uiBlock *UI_block_begin(const bContext *C,
                        Scene *scene,
                        wmWindow *window,
                        ARegion *region,
                        std::string name,
                        blender::ui::EmbossType emboss);
void UI_block_end_ex(const bContext *C,
                     Main *bmain,
                     wmWindow *window,
                     Scene *scene,
                     ARegion *region,
                     Depsgraph *depsgraph,
                     uiBlock *block,
                     const int xy[2] = nullptr,
                     int r_xy[2] = nullptr);
void UI_block_end(const bContext *C, uiBlock *block);
/**
 * Uses local copy of style, to scale things down, and allow widgets to change stuff.
 */
void UI_block_draw(const bContext *C, uiBlock *block);
void UI_blocklist_update_window_matrix(const bContext *C, const ListBase *lb);
void UI_blocklist_update_view_for_buttons(const bContext *C, const ListBase *lb);
void UI_blocklist_draw(const bContext *C, const ListBase *lb);
void UI_block_update_from_old(const bContext *C, uiBlock *block);

enum {
  UI_BLOCK_THEME_STYLE_REGULAR = 0,
  UI_BLOCK_THEME_STYLE_POPUP = 1,
};
void UI_block_theme_style_set(uiBlock *block, char theme_style);
blender::ui::EmbossType UI_block_emboss_get(uiBlock *block);
void UI_block_emboss_set(uiBlock *block, blender::ui::EmbossType emboss);
bool UI_block_is_search_only(const uiBlock *block);
/**
 * Use when a block must be searched to give accurate results
 * for the whole region but shouldn't be displayed.
 */
void UI_block_set_search_only(uiBlock *block, bool search_only);

/**
 * Used for operator presets.
 */
void UI_block_set_active_operator(uiBlock *block, wmOperator *op, const bool free);

/**
 * Can be called with C==NULL.
 */
void UI_block_free(const bContext *C, uiBlock *block);

void UI_block_listen(const uiBlock *block, const wmRegionListenerParams *listener_params);

/**
 * Can be called with C==NULL.
 */
void UI_blocklist_free(const bContext *C, ARegion *region);
void UI_blocklist_free_inactive(const bContext *C, ARegion *region);

/**
 * Is called by notifier.
 */
void UI_screen_free_active_but_highlight(const bContext *C, bScreen *screen);
void UI_region_free_active_but_all(bContext *C, ARegion *region);

void UI_block_region_set(uiBlock *block, ARegion *region);

void UI_block_lock_set(uiBlock *block, bool val, const char *lockstr);
void UI_block_lock_clear(uiBlock *block);

#define UI_BUTTON_SECTION_MERGE_DISTANCE (UI_UNIT_X * 3)
/* Separator line between regions if the #uiButtonSectionsAlign is not #None. */
#define UI_BUTTON_SECTION_SEPERATOR_LINE_WITH (U.pixelsize * 2)

enum class uiButtonSectionsAlign : int8_t { None = 1, Top, Bottom };
/**
 * Draw a background with rounded corners behind each visual group of buttons. The visual groups
 * are separated by spacer buttons (#uiLayout::separator_spacer()). Button groups that are closer
 * than #UI_BUTTON_SECTION_MERGE_DISTANCE will be merged into one visual section. If the group is
 * closer than that to a region edge, it will also be extended to that, and the rounded corners
 * will be removed on that edge.
 *
 * \note This currently only works well for horizontal, header like regions.
 */
void UI_region_button_sections_draw(const ARegion *region,
                                    int /*THemeColorID*/ colorid,
                                    uiButtonSectionsAlign align);
bool UI_region_button_sections_is_inside_x(const ARegion *region, const int mval_x);

/**
 * Automatic aligning, horizontal or vertical.
 */
void UI_block_align_begin(uiBlock *block);
void UI_block_align_end(uiBlock *block);

/** Block bounds/position calculation. */
enum eBlockBoundsCalc {
  UI_BLOCK_BOUNDS_NONE = 0,
  UI_BLOCK_BOUNDS = 1,
  UI_BLOCK_BOUNDS_TEXT,
  UI_BLOCK_BOUNDS_POPUP_MOUSE,
  UI_BLOCK_BOUNDS_POPUP_MENU,
  UI_BLOCK_BOUNDS_POPUP_CENTER,
  UI_BLOCK_BOUNDS_PIE_CENTER,
};

/**
 * Used for various cases.
 */
void UI_block_bounds_set_normal(uiBlock *block, int addval);
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

int UI_blocklist_min_y_get(ListBase *lb);

void UI_block_direction_set(uiBlock *block, char direction);
/**
 * This call escapes if there's alignment flags.
 */
void UI_block_flag_enable(uiBlock *block, int flag);
void UI_block_flag_disable(uiBlock *block, int flag);
void UI_block_translate(uiBlock *block, float x, float y);

int UI_but_return_value_get(uiBut *but);

uiBut *UI_but_active_drop_name_button(const bContext *C);
/**
 * Returns true if highlighted button allows drop of names.
 * called in region context.
 */
bool UI_but_active_drop_name(const bContext *C);
bool UI_but_active_drop_color(bContext *C);

void UI_but_flag_enable(uiBut *but, int flag);
void UI_but_flag_disable(uiBut *but, int flag);
bool UI_but_flag_is_set(uiBut *but, int flag);
void UI_but_flag2_enable(uiBut *but, int flag);

void UI_but_drawflag_enable(uiBut *but, int flag);
void UI_but_drawflag_disable(uiBut *but, int flag);

void UI_but_dragflag_enable(uiBut *but, int flag);
void UI_but_dragflag_disable(uiBut *but, int flag);

void UI_but_disable(uiBut *but, const char *disabled_hint);

void UI_but_type_set_menu_from_pulldown(uiBut *but);

/**
 * Sets the button's color, normally only used to recolor the icon. In the
 * special case of ButType::Label without icon this is used as text color.
 */
void UI_but_color_set(uiBut *but, const uchar color[4]);

bool UI_but_is_color_gamma(uiBut &but);
const ColorManagedDisplay *UI_but_cm_display_get(uiBut &but);

/**
 * Set at hint that describes the expected value when empty.
 */
void UI_but_placeholder_set(uiBut *but, blender::StringRef placeholder_text);

/**
 * Special button case, only draw it when used actively, for outliner etc.
 *
 * Needed for temporarily rename buttons, such as in outliner or file-select,
 * they should keep calling #uiDefBut to keep them alive.
 * \return false when button removed.
 */
bool UI_but_active_only_ex(
    const bContext *C, ARegion *region, uiBlock *block, uiBut *but, bool remove_on_failure);
bool UI_but_active_only(const bContext *C, ARegion *region, uiBlock *block, uiBut *but);
/**
 * \warning This must run after other handlers have been added,
 * otherwise the handler won't be removed, see: #71112.
 */
bool UI_block_active_only_flagged_buttons(const bContext *C, ARegion *region, uiBlock *block);

/**
 * Simulate button click.
 */
void UI_but_execute(const bContext *C, ARegion *region, uiBut *but);

std::optional<std::string> UI_but_online_manual_id(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
std::optional<std::string> UI_but_online_manual_id_from_active(const bContext *C)
    ATTR_WARN_UNUSED_RESULT;
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
                uiButTypeWithPointerType but_and_ptr_type,
                int retval,
                blender::StringRef str,
                int x,
                int y,
                short width,
                short height,
                void *poin,
                float min,
                float max,
                std::optional<blender::StringRef> tip);
uiBut *uiDefButF(uiBlock *block,
                 ButType type,
                 int retval,
                 blender::StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 float *poin,
                 float min,
                 float max,
                 std::optional<blender::StringRef> tip);
uiBut *uiDefButI(uiBlock *block,
                 ButType type,
                 int retval,
                 blender::StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 int *poin,
                 float min,
                 float max,
                 std::optional<blender::StringRef> tip);
uiBut *uiDefButBitI(uiBlock *block,
                    ButType type,
                    int bit,
                    int retval,
                    blender::StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    int *poin,
                    float min,
                    float max,
                    std::optional<blender::StringRef> tip);
uiBut *uiDefButS(uiBlock *block,
                 ButType type,
                 int retval,
                 blender::StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 short *poin,
                 float min,
                 float max,
                 std::optional<blender::StringRef> tip);
uiBut *uiDefButBitS(uiBlock *block,
                    ButType type,
                    int bit,
                    int retval,
                    blender::StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    short *poin,
                    float min,
                    float max,
                    std::optional<blender::StringRef> tip);
uiBut *uiDefButC(uiBlock *block,
                 ButType type,
                 int retval,
                 blender::StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 char *poin,
                 float min,
                 float max,
                 std::optional<blender::StringRef> tip);
uiBut *uiDefButBitC(uiBlock *block,
                    ButType type,
                    int bit,
                    int retval,
                    blender::StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    char *poin,
                    float min,
                    float max,
                    std::optional<blender::StringRef> tip);
uiBut *uiDefButR(uiBlock *block,
                 ButType type,
                 int retval,
                 std::optional<blender::StringRef> str,
                 int x,
                 int y,
                 short width,
                 short height,
                 PointerRNA *ptr,
                 blender::StringRefNull propname,
                 int index,
                 float min,
                 float max,
                 std::optional<blender::StringRef> tip);
uiBut *uiDefButR_prop(uiBlock *block,
                      ButType type,
                      int retval,
                      std::optional<blender::StringRef> str,
                      int x,
                      int y,
                      short width,
                      short height,
                      PointerRNA *ptr,
                      PropertyRNA *prop,
                      int index,
                      float min,
                      float max,
                      std::optional<blender::StringRef> tip);
uiBut *uiDefButO(uiBlock *block,
                 ButType type,
                 blender::StringRefNull opname,
                 blender::wm::OpCallContext opcontext,
                 const std::optional<blender::StringRef> str,
                 int x,
                 int y,
                 short width,
                 short height,
                 std::optional<blender::StringRef> tip);
uiBut *uiDefButO_ptr(uiBlock *block,
                     ButType type,
                     wmOperatorType *ot,
                     blender::wm::OpCallContext opcontext,
                     blender::StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     std::optional<blender::StringRef> tip);

uiBut *uiDefIconBut(uiBlock *block,
                    uiButTypeWithPointerType but_and_ptr_type,
                    int retval,
                    int icon,
                    int x,
                    int y,
                    short width,
                    short height,
                    void *poin,
                    float min,
                    float max,
                    std::optional<blender::StringRef> tip);
uiBut *uiDefIconButI(uiBlock *block,
                     ButType type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     int *poin,
                     float min,
                     float max,
                     std::optional<blender::StringRef> tip);
uiBut *uiDefIconButBitI(uiBlock *block,
                        ButType type,
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
                        std::optional<blender::StringRef> tip);
uiBut *uiDefIconButS(uiBlock *block,
                     ButType type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     short *poin,
                     float min,
                     float max,
                     std::optional<blender::StringRef> tip);
uiBut *uiDefIconButBitS(uiBlock *block,
                        ButType type,
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
                        std::optional<blender::StringRef> tip);
uiBut *uiDefIconButBitC(uiBlock *block,
                        ButType type,
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
                        std::optional<blender::StringRef> tip);
uiBut *uiDefIconButR(uiBlock *block,
                     ButType type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     PointerRNA *ptr,
                     blender::StringRefNull propname,
                     int index,
                     float min,
                     float max,
                     std::optional<blender::StringRef> tip);
uiBut *uiDefIconButR_prop(uiBlock *block,
                          ButType type,
                          int retval,
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
                          std::optional<blender::StringRef> tip);
uiBut *uiDefIconButO(uiBlock *block,
                     ButType type,
                     blender::StringRefNull opname,
                     blender::wm::OpCallContext opcontext,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     std::optional<blender::StringRef> tip);
uiBut *uiDefIconButO_ptr(uiBlock *block,
                         ButType type,
                         wmOperatorType *ot,
                         blender::wm::OpCallContext opcontext,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         std::optional<blender::StringRef> tip);
uiBut *uiDefIconPreviewBut(uiBlock *block,
                           ButType type,
                           int retval,
                           int icon,
                           int x,
                           int y,
                           short width,
                           short height,
                           void *poin,
                           float min,
                           float max,
                           std::optional<blender::StringRef> tip);
uiBut *uiDefButImage(
    uiBlock *block, void *imbuf, int x, int y, short width, short height, const uchar color[4]);
uiBut *uiDefButAlert(uiBlock *block, int icon, int x, int y, short width, short height);
/** Button containing both string label and icon. */
uiBut *uiDefIconTextBut(uiBlock *block,
                        uiButTypeWithPointerType but_and_ptr_type,
                        int retval,
                        int icon,
                        blender::StringRef str,
                        int x,
                        int y,
                        short width,
                        short height,
                        void *poin,
                        std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextButI(uiBlock *block,
                         ButType type,
                         int retval,
                         int icon,
                         blender::StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         int *poin,
                         std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextButS(uiBlock *block,
                         ButType type,
                         int retval,
                         int icon,
                         blender::StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         short *poin,
                         std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextButR(uiBlock *block,
                         ButType type,
                         int retval,
                         int icon,
                         std::optional<blender::StringRefNull> str,
                         int x,
                         int y,
                         short width,
                         short height,
                         PointerRNA *ptr,
                         blender::StringRefNull propname,
                         int index,
                         std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextButR_prop(uiBlock *block,
                              ButType type,
                              int retval,
                              int icon,
                              std::optional<blender::StringRef> str,
                              int x,
                              int y,
                              short width,
                              short height,
                              PointerRNA *ptr,
                              PropertyRNA *prop,
                              int index,
                              float min,
                              float max,
                              std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextButO(uiBlock *block,
                         ButType type,
                         blender::StringRefNull,
                         blender::wm::OpCallContext opcontext,
                         int icon,
                         blender::StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextButO_ptr(uiBlock *block,
                             ButType type,
                             wmOperatorType *ot,
                             blender::wm::OpCallContext opcontext,
                             int icon,
                             blender::StringRef str,
                             int x,
                             int y,
                             short width,
                             short height,
                             std::optional<blender::StringRef> tip);

void UI_but_operator_set(uiBut *but,
                         wmOperatorType *optype,
                         blender::wm::OpCallContext opcontext,
                         const PointerRNA *opptr = nullptr);
/**
 * Disable calling operators from \a but in button handling. Useful to attach an operator to a
 * button for tooltips, "Assign Shortcut", etc. without actually making the button execute the
 * operator.
 */
void UI_but_operator_set_never_call(uiBut *but);

/** For passing inputs to ButO buttons. */
PointerRNA *UI_but_operator_ptr_ensure(uiBut *but);

void UI_but_context_ptr_set(uiBlock *block,
                            uiBut *but,
                            blender::StringRef name,
                            const PointerRNA *ptr);
void UI_but_context_int_set(uiBlock *block, uiBut *but, blender::StringRef name, int64_t value);
const PointerRNA *UI_but_context_ptr_get(const uiBut *but,
                                         blender::StringRef name,
                                         const StructRNA *type = nullptr);
std::optional<blender::StringRefNull> UI_but_context_string_get(const uiBut *but,
                                                                blender::StringRef name);
std::optional<int64_t> UI_but_context_int_get(const uiBut *but, blender::StringRef name);
const bContextStore *UI_but_context_get(const uiBut *but);

void UI_but_unit_type_set(uiBut *but, int unit_type);
int UI_but_unit_type_get(const uiBut *but);

std::optional<EnumPropertyItem> UI_but_rna_enum_item_get(bContext &C, uiBut &but);

std::string UI_but_string_get_rna_property_identifier(const uiBut &but);
std::string UI_but_string_get_rna_struct_identifier(const uiBut &but);
std::string UI_but_string_get_label(uiBut &but);
std::string UI_but_context_menu_title_from_button(uiBut &but);
/**
 * Query the result of #uiBut::tip_label_func().
 * Meant to allow overriding the label to be displayed in the tool-tip.
 */
std::string UI_but_string_get_tooltip_label(const uiBut &but);
std::string UI_but_string_get_rna_label(uiBut &but);
/** Context specified in `CTX_*_` macros are just unreachable! */
std::string UI_but_string_get_rna_label_context(const uiBut &but);
std::string UI_but_string_get_tooltip(bContext &C, uiBut &but);
std::string UI_but_string_get_rna_tooltip(bContext &C, uiBut &but);
/** Buttons assigned to an operator (common case). */
std::string UI_but_string_get_operator_keymap(bContext &C, uiBut &but);
/** Use for properties that are bound to one of the context cycle, etc. keys. */
std::string UI_but_string_get_property_keymap(bContext &C, uiBut &but);

std::string UI_but_extra_icon_string_get_label(const uiButExtraOpIcon &extra_icon);
std::string UI_but_extra_icon_string_get_tooltip(bContext &C, const uiButExtraOpIcon &extra_icon);
std::string UI_but_extra_icon_string_get_operator_keymap(const bContext &C,
                                                         const uiButExtraOpIcon &extra_icon);

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

/***************************** ID Utilities *******************************/

int UI_icon_from_id(const ID *id);
/** See: #BKE_report_type_str */
int UI_icon_from_report_type(int type);
int UI_icon_colorid_from_report_type(int type);
int UI_text_colorid_from_report_type(int type);

int UI_icon_from_event_type(short event_type, short event_value);
int UI_icon_from_keymap_item(const wmKeyMapItem *kmi, int r_icon_mod[KM_MOD_NUM]);

uiBut *uiDefMenuBut(uiBlock *block,
                    uiMenuCreateFunc func,
                    void *arg,
                    blender::StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    std::optional<blender::StringRef> tip);
uiBut *uiDefIconTextMenuBut(uiBlock *block,
                            uiMenuCreateFunc func,
                            void *arg,
                            int icon,
                            blender::StringRef str,
                            int x,
                            int y,
                            short width,
                            short height,
                            std::optional<blender::StringRef> tip);
uiBut *uiDefIconMenuBut(uiBlock *block,
                        uiMenuCreateFunc func,
                        void *arg,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        std::optional<blender::StringRef> tip);

uiBut *uiDefBlockBut(uiBlock *block,
                     uiBlockCreateFunc func,
                     void *arg,
                     blender::StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     std::optional<blender::StringRef> tip);
uiBut *uiDefBlockButN(uiBlock *block,
                      uiBlockCreateFunc func,
                      void *argN,
                      blender::StringRef str,
                      int x,
                      int y,
                      short width,
                      short height,
                      std::optional<blender::StringRef> tip,
                      uiButArgNFree func_argN_free_fn = MEM_freeN,
                      uiButArgNCopy func_argN_copy_fn = MEM_dupallocN);

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
                         std::optional<blender::StringRef> tip);

/**
 * \param arg: A pointer to string/name, use #UI_but_func_search_set() below to make this work.
 */
uiBut *uiDefSearchBut(uiBlock *block,
                      void *arg,
                      int retval,
                      int icon,
                      int maxncpy,
                      int x,
                      int y,
                      short width,
                      short height,
                      std::optional<blender::StringRef> tip);
/**
 * Same parameters as for #uiDefSearchBut, with additional operator type and properties,
 * used by callback to call again the right op with the right options (properties values).
 */
uiBut *uiDefSearchButO_ptr(uiBlock *block,
                           wmOperatorType *ot,
                           IDProperty *properties,
                           void *arg,
                           int retval,
                           int icon,
                           int maxncpy,
                           int x,
                           int y,
                           short width,
                           short height,
                           std::optional<blender::StringRef> tip);

/** For #uiDefAutoButsRNA. */
enum eButLabelAlign {
  /** Keep current layout for aligning label with property button. */
  UI_BUT_LABEL_ALIGN_NONE,
  /** Align label and property button vertically. */
  UI_BUT_LABEL_ALIGN_COLUMN,
  /** Split layout into a column for the label and one for property button. */
  UI_BUT_LABEL_ALIGN_SPLIT_COLUMN,
};

/** Return info for uiDefAutoButsRNA. */
enum eAutoPropButsReturn {
  /** Returns when no buttons were added */
  UI_PROP_BUTS_NONE_ADDED = 1 << 0,
  /** Returned when any property failed the custom check callback (check_prop) */
  UI_PROP_BUTS_ANY_FAILED_CHECK = 1 << 1,
};

ENUM_OPERATORS(eAutoPropButsReturn);

uiBut *uiDefAutoButR(uiBlock *block,
                     PointerRNA *ptr,
                     PropertyRNA *prop,
                     int index,
                     std::optional<blender::StringRef> name,
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
void UI_but_func_identity_compare_set(uiBut *but, uiButIdentityCompareFunc cmp_fn);

/**
 * Public function exported for functions that use #ButType::SearchMenu.
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
                        blender::StringRef name,
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

#define UI_SEARCHBOX_BOUNDS (6.0f * UI_SCALE_FAC)
#define UI_SEARCHBOX_TRIA_H (12.0f * UI_SCALE_FAC)
/**
 * Height in pixels, it's using hard-coded values still.
 */
int UI_searchbox_size_y();
int UI_searchbox_size_x();
/**
 * Guess a good width for the search box based on the searchable items.
 *
 * \note When used with a menu that does full refreshes, it might be beneficial to cache this size
 * because recomputing it is potentially expensive.
 */
int UI_searchbox_size_x_guess(const bContext *C, const uiButSearchUpdateFn update_fn, void *arg);
/**
 * Check if a string is in an existing search box.
 */
int UI_search_items_find_index(const uiSearchItems *items, const char *name);

/**
 * Adds a hint to the button which draws right aligned, grayed out and never clipped.
 */
void UI_but_hint_drawstr_set(uiBut *but, const char *string);
void UI_but_icon_indicator_number_set(uiBut *but, const int indicator_number);
void UI_but_icon_indicator_set(uiBut *but, const char *string);
void UI_but_icon_indicator_color_set(uiBut *but, const uchar color[4]);

void UI_but_node_link_set(uiBut *but, bNodeSocket *socket, const float draw_color[4]);

void UI_but_number_step_size_set(uiBut *but, float step_size);
void UI_but_number_precision_set(uiBut *but, float precision);

void UI_but_number_slider_step_size_set(uiBut *but, float step_size);
void UI_but_number_slider_precision_set(uiBut *but, float precision);

void UI_but_label_alpha_factor_set(uiBut *but, float alpha_factor);

void UI_but_search_preview_grid_size_set(uiBut *but, int rows, int cols);

void UI_but_view_item_draw_size_set(uiBut *but,
                                    const std::optional<int> draw_width = std::nullopt,
                                    const std::optional<int> draw_height = std::nullopt);

void UI_block_func_handle_set(uiBlock *block, uiBlockHandleFunc func, void *arg);
void UI_block_func_set(uiBlock *block, uiButHandleFunc func, void *arg1, void *arg2);
void UI_block_funcN_set(uiBlock *block,
                        uiButHandleNFunc funcN,
                        void *argN,
                        void *arg2,
                        uiButArgNFree func_argN_free_fn = MEM_freeN,
                        uiButArgNCopy func_argN_copy_fn = MEM_dupallocN);

void UI_but_func_rename_set(uiBut *but, uiButHandleRenameFunc func, void *arg1);
void UI_but_func_rename_full_set(uiBut *but,
                                 std::function<void(std::string &new_name)> rename_full_func);
void UI_but_func_set(uiBut *but, uiButHandleFunc func, void *arg1, void *arg2);
void UI_but_funcN_set(uiBut *but,
                      uiButHandleNFunc funcN,
                      void *argN,
                      void *arg2,
                      uiButArgNFree func_argN_free_fn = MEM_freeN,
                      uiButArgNCopy func_argN_copy_fn = MEM_dupallocN);

void UI_but_func_complete_set(uiBut *but, uiButCompleteFunc func, void *arg);

void UI_but_func_drawextra_set(uiBlock *block,
                               std::function<void(const bContext *C, rcti *rect)> func);

void UI_but_func_menu_step_set(uiBut *but, uiMenuStepFunc func);

/**
 * When a button displays a menu, hovering another button that can display one will switch to that
 * menu instead. In some cases that's unexpected, so the feature can be disabled here (as in, this
 * button will not spawn its menu on hover and the previously spawned menu will remain open).
 */
void UI_but_menu_disable_hover_open(uiBut *but);

void UI_but_func_tooltip_set(uiBut *but, uiButToolTipFunc func, void *arg, uiFreeArgFunc free_arg);
/**
 * Enable a tooltip that appears faster than the usual tooltip. If the button has both a quick and
 * a normal tooltip, the quick one is shown first, and expanded to the full one after the usual
 * tooltip delay. Quick tooltips are useful in cases like:
 * - A button doesn't show a label to save space but the label is still relevant. Show the label as
 *   quick tooltip in that case (like the name of tools in a compact, icon only tool-shelf).
 * - The only purpose of a button is to display this tooltip (like a warning icon with the warning
 *   text in the tooltip).
 */
void UI_but_func_quick_tooltip_set(uiBut *but, std::function<std::string(const uiBut *but)> func);

enum uiTooltipStyle {
  UI_TIP_STYLE_NORMAL = 0, /* Regular text. */
  UI_TIP_STYLE_HEADER,     /* Header text. */
  UI_TIP_STYLE_MONO,       /* Mono-spaced text. */
  UI_TIP_STYLE_IMAGE,      /* Image field. */
  UI_TIP_STYLE_SPACER,     /* Padding to separate sections. */
};

enum uiTooltipColorID {
  UI_TIP_LC_MAIN = 0, /* Color of primary text. */
  UI_TIP_LC_VALUE,    /* Color for the value of buttons (also shortcuts). */
  UI_TIP_LC_ACTIVE,   /* Color of titles of active enum values. */
  UI_TIP_LC_NORMAL,   /* Color of regular text. */
  UI_TIP_LC_PYTHON,   /* Color of python snippets. */
  UI_TIP_LC_ALERT,    /* Warning text color, eg: why operator can't run. */
  UI_TIP_LC_MAX
};

enum class uiTooltipImageBackground {
  None = 0,
  Checkerboard_Themed,
  Checkerboard_Fixed,
};

struct uiTooltipImage {
  ImBuf *ibuf = nullptr;
  short width = 0;
  short height = 0;
  bool premultiplied = false;
  bool border = false;
  bool text_color = false;
  uiTooltipImageBackground background = uiTooltipImageBackground::None;
};

void UI_but_func_tooltip_custom_set(uiBut *but,
                                    uiButToolTipCustomFunc func,
                                    void *arg,
                                    uiFreeArgFunc free_arg);

/**
 * \param text: Allocated text (transfer ownership to `data`) or null.
 * \param suffix: Allocated text (transfer ownership to `data`) or null.
 */
void UI_tooltip_text_field_add(uiTooltipData &data,
                               std::string text,
                               std::string suffix,
                               const uiTooltipStyle style,
                               const uiTooltipColorID color_id,
                               const bool is_pad = false);

/**
 * \param image: Image buffer (duplicated, ownership is *not* transferred to `data`).
 * \param image_size: Display size for the image (pixels without UI scale applied).
 */
void UI_tooltip_image_field_add(uiTooltipData &data, const uiTooltipImage &image_data);

void UI_tooltip_color_field_add(uiTooltipData &data,
                                const blender::float4 &color,
                                bool has_alpha,
                                bool is_gamma,
                                const ColorManagedDisplay *display,
                                uiTooltipColorID color_id);

/**
 * Add Python-related information to the tooltip. The caller is responsible for checking
 * #USER_TOOLTIPS_PYTHON.
 */
void UI_tooltip_uibut_python_add(uiTooltipData &data,
                                 bContext &C,
                                 uiBut &but,
                                 uiButExtraOpIcon *extra_icon);

/**
 * Recreate tool-tip (use to update dynamic tips)
 */
void UI_but_tooltip_refresh(bContext *C, uiBut *but);
/**
 * Removes tool-tip timer from active but
 * (meaning tool-tip is disabled until it's re-enabled again).
 */
void UI_but_tooltip_timer_remove(bContext *C, uiBut *but);

bool UI_textbutton_activate_rna(const bContext *C,
                                ARegion *region,
                                const void *rna_poin_data,
                                const char *rna_prop_id);
bool UI_textbutton_activate_but(const bContext *C, uiBut *actbut);

/**
 * push a new event onto event queue to activate the given button
 * (usually a text-field) upon entering a popup
 */
void UI_but_focus_on_enter_event(wmWindow *win, uiBut *but);

void UI_but_func_hold_set(uiBut *but, uiButHandleHoldFunc func, void *argN);

PointerRNA *UI_but_extra_operator_icon_add(uiBut *but,
                                           blender::StringRefNull opname,
                                           blender::wm::OpCallContext opcontext,
                                           int icon);
wmOperatorType *UI_but_extra_operator_icon_optype_get(const uiButExtraOpIcon *extra_icon);
PointerRNA *UI_but_extra_operator_icon_opptr_get(const uiButExtraOpIcon *extra_icon);

/**
 * Get the scaled size for a preview button (typically #UI_BTyPE_PREVIEW_TILE) based on \a
 * size_px plus padding.
 */
int UI_preview_tile_size_x(const int size_px = 96);
int UI_preview_tile_size_y(const int size_px = 96);
int UI_preview_tile_size_y_no_label(const int size_px = 96);

/* Autocomplete
 *
 * Tab complete helper functions, for use in uiButCompleteFunc callbacks.
 * Call begin once, then multiple times do_name with all possibilities,
 * and finally end to finish and get the completed name. */

struct AutoComplete;

#define AUTOCOMPLETE_NO_MATCH 0
#define AUTOCOMPLETE_FULL_MATCH 1
#define AUTOCOMPLETE_PARTIAL_MATCH 2

AutoComplete *UI_autocomplete_begin(const char *startname, size_t maxncpy);
void UI_autocomplete_update_name(AutoComplete *autocpl, blender::StringRef name);
int UI_autocomplete_end(AutoComplete *autocpl, char *autoname);

/* Button drag-data (interface_drag.cc).
 *
 * Functions to set drag data for buttons. This enables dragging support, whereby the drag data is
 * "dragged", not the button itself. */

void UI_but_drag_set_id(uiBut *but, ID *id);
/**
 * Set an image to display while dragging. This works for any drag type (`WM_DRAG_XXX`).
 * Not to be confused with #UI_but_drag_set_image(), which sets up dragging of an image.
 *
 * Sets #UI_BUT_DRAG_FULL_BUT so the full button can be dragged.
 */
void UI_but_drag_attach_image(uiBut *but, const ImBuf *imb, float scale);

/**
 * Sets #UI_BUT_DRAG_FULL_BUT so the full button can be dragged.
 * \param asset: May be passed from a temporary variable, drag data only stores a copy of this.
 * \param icon: Small icon that will be drawn while dragging.
 * \param preview_icon: Bigger preview size icon that will be drawn while dragging instead of \a
 * icon.
 */
void UI_but_drag_set_asset(uiBut *but,
                           const blender::asset_system::AssetRepresentation *asset,
                           const AssetImportSettings &import_settings,
                           int icon,
                           int preview_icon);

void UI_but_drag_set_rna(uiBut *but, PointerRNA *ptr);
/**
 * Enable dragging a path from this button.
 * \param path: The path to drag. The passed string may be destructed, button keeps a copy.
 */
void UI_but_drag_set_path(uiBut *but, const char *path);
void UI_but_drag_set_name(uiBut *but, const char *name);

/**
 * Sets #UI_BUT_DRAG_FULL_BUT so the full button can be dragged.
 * \param path: The path to drag. The passed string may be destructed, button keeps a copy.
 */
void UI_but_drag_set_image(uiBut *but, const char *path, int icon, const ImBuf *imb, float scale);

/* Panels
 *
 * Functions for creating, freeing and drawing panels. The API here
 * could use a good cleanup, though how they will function in 2.5 is
 * not clear yet so we postpone that. */

void UI_panels_begin(const bContext *C, ARegion *region);
void UI_panels_end(const bContext *C, ARegion *region, int *r_x, int *r_y);
/**
 * Draw panels, selected (panels currently being dragged) on top.
 */
void UI_panels_draw(const bContext *C, ARegion *region);

Panel *UI_panel_find_by_type(ListBase *lb, const PanelType *pt);
/**
 * \note \a panel should be return value from #UI_panel_find_by_type and can be NULL.
 */
Panel *UI_panel_begin(
    ARegion *region, ListBase *lb, uiBlock *block, PanelType *pt, Panel *panel, bool *r_open);
/**
 * Create the panel header button group, used to mark which buttons are part of
 * panel headers for the panel search process that happens later. This Should be
 * called before adding buttons for the panel's header layout.
 */
void UI_panel_header_buttons_begin(Panel *panel);
/**
 * Finish the button group for the panel header to avoid putting panel body buttons in it.
 */
void UI_panel_header_buttons_end(Panel *panel);
void UI_panel_end(Panel *panel, int width, int height);

/** Set the name that should be drawn in the UI. Should be a translated string. */
void UI_panel_drawname_set(Panel *panel, blender::StringRef name);

/**
 * Set a context for this entire panel and its current layout. This should be used whenever panel
 * callbacks that are called outside of regular drawing might require context. Currently it affects
 * the #PanelType.reorder callback only.
 */
void UI_panel_context_pointer_set(Panel *panel, const char *name, PointerRNA *ptr);

/**
 * Get the panel's expansion state, taking into account
 * expansion set from property search if it applies.
 */
bool UI_panel_is_closed(const Panel *panel);
bool UI_panel_is_active(const Panel *panel);
/**
 * For button layout next to label.
 */
void UI_panel_label_offset(const uiBlock *block, int *r_x, int *r_y);
bool UI_panel_should_show_background(const ARegion *region, const PanelType *panel_type);
int UI_panel_size_y(const Panel *panel);
bool UI_panel_is_dragging(const Panel *panel);
/**
 * Find whether a panel or any of its sub-panels contain a property that matches the search filter,
 * depending on the search process running in #UI_block_apply_search_filter earlier.
 */
bool UI_panel_matches_search_filter(const Panel *panel);
bool UI_panel_can_be_pinned(const Panel *panel);

bool UI_panel_category_is_visible(const ARegion *region);
void UI_panel_category_add(ARegion *region, const char *name);
PanelCategoryDyn *UI_panel_category_find(const ARegion *region, const char *idname);
int UI_panel_category_index_find(ARegion *region, const char *idname);
PanelCategoryStack *UI_panel_category_active_find(ARegion *region, const char *idname);
const char *UI_panel_category_active_get(ARegion *region, bool set_fallback);
void UI_panel_category_active_set(ARegion *region, const char *idname);
/** \param index: index of item _in #ARegion.panels_category list_. */
void UI_panel_category_index_active_set(ARegion *region, const int index);
void UI_panel_category_active_set_default(ARegion *region, const char *idname);
void UI_panel_category_clear_all(ARegion *region);
/**
 * Draw vertical tabs on the left side of the region, one tab per category.
 */
void UI_panel_category_draw_all(ARegion *region, const char *category_id_active);

void UI_panel_stop_animation(const bContext *C, Panel *panel);

/* Panel custom data. */
PointerRNA *UI_panel_custom_data_get(const Panel *panel);
PointerRNA *UI_region_panel_custom_data_under_cursor(const bContext *C, const wmEvent *event);
void UI_panel_custom_data_set(Panel *panel, PointerRNA *custom_data);

/* Poly-instantiated panels for representing a list of data. */
/**
 * Called in situations where panels need to be added dynamically rather than
 * having only one panel corresponding to each #PanelType.
 */
Panel *UI_panel_add_instanced(const bContext *C,
                              ARegion *region,
                              ListBase *panels,
                              const char *panel_idname,
                              PointerRNA *custom_data);
/**
 * Remove instanced panels from the region's panel list.
 *
 * \note Can be called with NULL \a C, but it should be avoided because
 * handlers might not be removed.
 */
void UI_panels_free_instanced(const bContext *C, ARegion *region);

#define INSTANCED_PANEL_UNIQUE_STR_SIZE 16
/**
 * Find a unique key to append to the #PanelType.idname for the lookup to the panel's #uiBlock.
 * Needed for instanced panels, where there can be multiple with the same type and identifier.
 */
void UI_list_panel_unique_str(Panel *panel, char *r_name);

using uiListPanelIDFromDataFunc = void (*)(void *data_link, char *r_idname);
/**
 * Check if the instanced panels in the region's panels correspond to the list of data the panels
 * represent. Returns false if the panels have been reordered or if the types from the list data
 * don't match in any way.
 *
 * \param data: The list of data to check against the instanced panels.
 * \param panel_idname_func: Function to find the #PanelType.idname for each item in the data list.
 * For a readability and generality, this lookup happens separately for each type of panel list.
 */
bool UI_panel_list_matches_data(ARegion *region,
                                ListBase *data,
                                uiListPanelIDFromDataFunc panel_idname_func);

/* Handlers
 *
 * Handlers that can be registered in regions, areas and windows for
 * handling WM events. Mostly this is done automatic by modules such
 * as screen/ if ED_KEYMAP_UI is set, or internally in popup functions. */

void UI_region_handlers_add(ListBase *handlers);
void UI_popup_handlers_add(bContext *C, ListBase *handlers, uiPopupBlockHandle *popup, char flag);
void UI_popup_handlers_remove(ListBase *handlers, uiPopupBlockHandle *popup);
void UI_popup_handlers_remove_all(bContext *C, ListBase *handlers);

/* Module
 *
 * init and exit should be called before using this module. init_userdef must
 * be used to reinitialize some internal state if user preferences change. */

void UI_init();
/* after reading userdef file */
void UI_init_userdef();
void UI_reinit_font();
void UI_exit();

/* When changing UI font, update text style weights with default font weight
 * if non-variable. Therefore fixed weight bold font will look bold. */
void UI_update_text_styles();

#define UI_UNIT_X ((void)0, U.widget_unit)
#define UI_UNIT_Y ((void)0, U.widget_unit)

#define UI_HEADER_OFFSET \
  ((void)0, ((U.uiflag & USER_AREA_CORNER_HANDLE) ? 16.0f : 8.0f) * UI_SCALE_FAC)

#define UI_AZONESPOTW_LEFT UI_HEADER_OFFSET       /* Width of left-side corner #AZone. */
#define UI_AZONESPOTW_RIGHT (8.0f * UI_SCALE_FAC) /* Width of right-side corner #AZone. */
#define UI_AZONESPOTH (0.6f * U.widget_unit)      /* Height of corner action zone #AZone. */

/* uiLayoutOperatorButs flags */
enum {
  UI_TEMPLATE_OP_PROPS_SHOW_TITLE = 1 << 0,
  UI_TEMPLATE_OP_PROPS_SHOW_EMPTY = 1 << 1,
  UI_TEMPLATE_OP_PROPS_COMPACT = 1 << 2,
  UI_TEMPLATE_OP_PROPS_HIDE_ADVANCED = 1 << 3,
  /* Disable property split for the default layout (custom ui callbacks still have full control
   * over the layout and can enable it). */
  UI_TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT = 1 << 4,
  UI_TEMPLATE_OP_PROPS_HIDE_PRESETS = 1 << 5,
  /**
   * Allow the buttons placed by the template to send an undo push. Usually this isn't wanted,
   * except for rare cases where operators draw their properties into a regular UI for later
   * execution (e.g. collection exporter panels in Properties).
   *
   * This should never be enabled for UIs that trigger redo, like "Adjust Last Operation" panels.
   */
  UI_TEMPLATE_OP_PROPS_ALLOW_UNDO_PUSH = 1 << 6,
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

void UI_region_message_subscribe(ARegion *region, wmMsgBus *mbus);

/**
 * This is a bit of a hack but best keep it in one place at least.
 */
wmOperatorType *UI_but_operatortype_get_from_enum_menu(uiBut *but, PropertyRNA **r_prop);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
MenuType *UI_but_menutype_get(const uiBut *but);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
PanelType *UI_but_paneltype_get(const uiBut *but);
/**
 * This is a bit of a hack but best keep it in one place at least.
 */
std::optional<blender::StringRefNull> UI_but_asset_shelf_type_idname_get(const uiBut *but);

/* templates */
void uiTemplateHeader(uiLayout *layout, bContext *C);
void uiTemplateID(uiLayout *layout,
                  const bContext *C,
                  PointerRNA *ptr,
                  blender::StringRefNull propname,
                  const char *newop,
                  const char *openop,
                  const char *unlinkop,
                  int filter = UI_TEMPLATE_ID_FILTER_ALL,
                  bool live_icon = false,
                  std::optional<blender::StringRef> text = std::nullopt);
void uiTemplateIDBrowse(uiLayout *layout,
                        bContext *C,
                        PointerRNA *ptr,
                        blender::StringRefNull propname,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        int filter = UI_TEMPLATE_ID_FILTER_ALL,
                        const char *text = nullptr);
void uiTemplateIDPreview(uiLayout *layout,
                         bContext *C,
                         PointerRNA *ptr,
                         blender::StringRefNull propname,
                         const char *newop,
                         const char *openop,
                         const char *unlinkop,
                         int rows,
                         int cols,
                         int filter = UI_TEMPLATE_ID_FILTER_ALL,
                         bool hide_buttons = false);
void uiTemplateMatrix(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
/**
 * Version of #uiTemplateID using tabs.
 */
void uiTemplateIDTabs(uiLayout *layout,
                      bContext *C,
                      PointerRNA *ptr,
                      blender::StringRefNull propname,
                      const char *newop,
                      const char *menu,
                      int filter = UI_TEMPLATE_ID_FILTER_ALL);
/**
 * This is for selecting the type of ID-block to use,
 * and then from the relevant type choosing the block to use.
 *
 * \param propname: property identifier for property that ID-pointer gets stored to.
 * \param proptypename: property identifier for property
 * used to determine the type of ID-pointer that can be used.
 */
void uiTemplateAnyID(uiLayout *layout,
                     PointerRNA *ptr,
                     blender::StringRefNull propname,
                     blender::StringRefNull proptypename,
                     std::optional<blender::StringRef> text);

/**
 * Action selector.
 *
 * This is a specialization of #uiTemplateID, hard-coded to assign Actions to the given ID.
 * Such a specialization is necessary, as the RNA property (`id.animation_data.action`) does not
 * exist when the ID's `adt` pointer is `nullptr`. In that case uiTemplateID will not be able
 * to find the RNA type of that property, which in turn it needs to determine the type of IDs to
 * show.
 */
void uiTemplateAction(uiLayout *layout,
                      const bContext *C,
                      ID *id,
                      const char *newop,
                      const char *unlinkop,
                      std::optional<blender::StringRef> text);

/**
 * Search menu to pick an item from a collection.
 * A version of uiTemplateID that works for non-ID types.
 */
void uiTemplateSearch(uiLayout *layout,
                      const bContext *C,
                      PointerRNA *ptr,
                      blender::StringRefNull propname,
                      PointerRNA *searchptr,
                      const char *searchpropname,
                      const char *newop,
                      const char *unlinkop,
                      std::optional<blender::StringRef> text = std::nullopt);
void uiTemplateSearchPreview(uiLayout *layout,
                             bContext *C,
                             PointerRNA *ptr,
                             blender::StringRefNull propname,
                             PointerRNA *searchptr,
                             const char *searchpropname,
                             const char *newop,
                             const char *unlinkop,
                             int rows,
                             int cols,
                             std::optional<blender::StringRef> text = std::nullopt);
/**
 * This is creating/editing RNA-Paths
 *
 * - ptr: struct which holds the path property
 * - propname: property identifier for property that path gets stored to
 * - root_ptr: struct that path gets built from
 */
void uiTemplatePathBuilder(uiLayout *layout,
                           PointerRNA *ptr,
                           blender::StringRefNull propname,
                           PointerRNA *root_ptr,
                           std::optional<blender::StringRefNull> text);
void uiTemplateModifiers(uiLayout *layout, bContext *C);
void uiTemplateStripModifiers(uiLayout *layout, bContext *C);
/**
 * Check if the shader effect panels don't match the data and rebuild the panels if so.
 */
void uiTemplateShaderFx(uiLayout *layout, bContext *C);
/**
 * Check if the constraint panels don't match the data and rebuild the panels if so.
 */
void uiTemplateConstraints(uiLayout *layout, bContext *C, bool use_bone_constraints);

uiLayout *uiTemplateGpencilModifier(uiLayout *layout, bContext *C, PointerRNA *ptr);
void uiTemplateGpencilColorPreview(uiLayout *layout,
                                   bContext *C,
                                   PointerRNA *ptr,
                                   blender::StringRefNull propname,
                                   int rows,
                                   int cols,
                                   float scale,
                                   int filter);

void uiTemplateOperatorRedoProperties(uiLayout *layout, const bContext *C);

void uiTemplateConstraintHeader(uiLayout *layout, PointerRNA *ptr);
void uiTemplatePreview(uiLayout *layout,
                       bContext *C,
                       ID *id,
                       bool show_buttons,
                       ID *parent,
                       MTex *slot,
                       const char *preview_id);
void uiTemplateColorRamp(uiLayout *layout,
                         PointerRNA *ptr,
                         blender::StringRefNull propname,
                         bool expand);
/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
void uiTemplateIcon(uiLayout *layout, int icon_value, float icon_scale);
/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
void uiTemplateIconView(uiLayout *layout,
                        PointerRNA *ptr,
                        blender::StringRefNull propname,
                        bool show_labels,
                        float icon_scale,
                        float icon_scale_popup);
void uiTemplateHistogram(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
void uiTemplateWaveform(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
void uiTemplateVectorscope(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
void uiTemplateCurveMapping(uiLayout *layout,
                            PointerRNA *ptr,
                            blender::StringRefNull propname,
                            int type,
                            bool levels,
                            bool brush,
                            bool neg_slope,
                            bool tone,
                            bool presets);
/**
 * Template for a path creation widget intended for custom bevel profiles.
 * This section is quite similar to #uiTemplateCurveMapping, but with reduced complexity.
 */
void uiTemplateCurveProfile(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
/**
 * This template now follows User Preference for type - name is not correct anymore.
 */
void uiTemplateColorPicker(uiLayout *layout,
                           PointerRNA *ptr,
                           blender::StringRefNull propname,
                           bool value_slider,
                           bool lock,
                           bool lock_luminosity,
                           bool cubic);
void uiTemplatePalette(uiLayout *layout,
                       PointerRNA *ptr,
                       blender::StringRefNull propname,
                       bool colors);
void uiTemplateCryptoPicker(uiLayout *layout,
                            PointerRNA *ptr,
                            blender::StringRefNull propname,
                            int icon);
/**
 * TODO: for now, grouping of layers is determined by dividing up the length of
 * the array of layer bit-flags.
 */
void uiTemplateLayers(uiLayout *layout,
                      PointerRNA *ptr,
                      blender::StringRefNull propname,
                      PointerRNA *used_ptr,
                      const char *used_propname,
                      int active_layer);
void uiTemplateImage(uiLayout *layout,
                     bContext *C,
                     PointerRNA *ptr,
                     blender::StringRefNull propname,
                     PointerRNA *userptr,
                     bool compact,
                     bool multiview);
void uiTemplateImageSettings(uiLayout *layout,
                             bContext *C,
                             PointerRNA *imfptr,
                             bool color_management,
                             const char *panel_idname = nullptr);
void uiTemplateImageStereo3d(uiLayout *layout, PointerRNA *stereo3d_format_ptr);
void uiTemplateImageViews(uiLayout *layout, PointerRNA *imaptr);
void uiTemplateImageFormatViews(uiLayout *layout, PointerRNA *imfptr, PointerRNA *ptr);
void uiTemplateImageLayers(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser);
void uiTemplateImageInfo(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser);
void uiTemplateRunningJobs(uiLayout *layout, bContext *C);
void UI_but_func_operator_search(uiBut *but);
void uiTemplateOperatorSearch(uiLayout *layout);

void UI_but_func_menu_search(uiBut *but, const char *single_menu_idname = nullptr);
void uiTemplateMenuSearch(uiLayout *layout);

/**
 * Draw Operator property buttons for redoing execution with different settings.
 * This function does not initialize the layout,
 * functions can be called on the layout before and after.
 */
void uiTemplateOperatorPropertyButs(
    const bContext *C, uiLayout *layout, wmOperator *op, eButLabelAlign label_align, short flag);
void uiTemplateHeader3D_mode(uiLayout *layout, bContext *C);
void uiTemplateEditModeSelection(uiLayout *layout, bContext *C);
void uiTemplateReportsBanner(uiLayout *layout, bContext *C);
void uiTemplateInputStatus(uiLayout *layout, bContext *C);
void uiTemplateStatusInfo(uiLayout *layout, bContext *C);
void uiTemplateKeymapItemProperties(uiLayout *layout, PointerRNA *ptr);

bool uiTemplateEventFromKeymapItem(uiLayout *layout,
                                   blender::StringRefNull text,
                                   const wmKeyMapItem *kmi,
                                   bool text_fallback);

/* Draw keymap item for status bar. Returns number of items consumed,
 * as X/Y/Z items may get merged to use less space. */
int uiTemplateStatusBarModalItem(uiLayout *layout,
                                 const wmKeyMap *keymap,
                                 const EnumPropertyItem *item);

void uiTemplateComponentMenu(uiLayout *layout,
                             PointerRNA *ptr,
                             blender::StringRefNull propname,
                             blender::StringRef name);
void uiTemplateNodeSocket(uiLayout *layout, bContext *C, const float color[4]);

/**
 * Draw the main CacheFile properties and operators (file path, scale, etc.), that is those which
 * do not have their own dedicated template functions.
 */
void uiTemplateCacheFile(uiLayout *layout,
                         const bContext *C,
                         PointerRNA *ptr,
                         blender::StringRefNull propname);

/**
 * Lookup the CacheFile PointerRNA of the given pointer and return it in the output parameter.
 * Returns true if `ptr` has a RNACacheFile, false otherwise. If false, the output parameter is not
 * initialized.
 */
bool uiTemplateCacheFilePointer(PointerRNA *ptr,
                                blender::StringRefNull propname,
                                PointerRNA *r_file_ptr);

/**
 * Draw the velocity related properties of the CacheFile.
 */
void uiTemplateCacheFileVelocity(uiLayout *layout, PointerRNA *fileptr);

/**
 * Draw the time related properties of the CacheFile.
 */
void uiTemplateCacheFileTimeSettings(uiLayout *layout, PointerRNA *fileptr);

/**
 * Draw the override layers related properties of the CacheFile.
 */
void uiTemplateCacheFileLayers(uiLayout *layout, const bContext *C, PointerRNA *fileptr);

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
};
ENUM_OPERATORS(uiTemplateListFlags);

void uiTemplateList(uiLayout *layout,
                    const bContext *C,
                    const char *listtype_name,
                    const char *list_id,
                    PointerRNA *dataptr,
                    blender::StringRefNull propname,
                    PointerRNA *active_dataptr,
                    const char *active_propname,
                    const char *item_dyntip_propname,
                    int rows,
                    int maxrows,
                    int layout_type,
                    int columns,
                    enum uiTemplateListFlags flags);
uiList *uiTemplateList_ex(uiLayout *layout,
                          const bContext *C,
                          const char *listtype_name,
                          const char *list_id,
                          PointerRNA *dataptr,
                          blender::StringRefNull propname,
                          PointerRNA *active_dataptr,
                          blender::StringRefNull active_propname,
                          const char *item_dyntip_propname,
                          int rows,
                          int maxrows,
                          int layout_type,
                          int columns,
                          enum uiTemplateListFlags flags,
                          void *customdata);

void uiTemplateNodeLink(
    uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input);
void uiTemplateNodeView(
    uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input);
void uiTemplateTextureUser(uiLayout *layout, bContext *C);
/**
 * Button to quickly show texture in Properties Editor texture tab.
 */
void uiTemplateTextureShow(uiLayout *layout,
                           const bContext *C,
                           PointerRNA *ptr,
                           PropertyRNA *prop);

void uiTemplateMovieClip(
    uiLayout *layout, bContext *C, PointerRNA *ptr, blender::StringRefNull propname, bool compact);
void uiTemplateTrack(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
void uiTemplateMarker(uiLayout *layout,
                      PointerRNA *ptr,
                      blender::StringRefNull propname,
                      PointerRNA *userptr,
                      PointerRNA *trackptr,
                      bool compact);
void uiTemplateMovieclipInformation(uiLayout *layout,
                                    PointerRNA *ptr,
                                    blender::StringRefNull propname,
                                    PointerRNA *userptr);

void uiTemplateColorspaceSettings(uiLayout *layout,
                                  PointerRNA *ptr,
                                  blender::StringRefNull propname);
void uiTemplateColormanagedViewSettings(uiLayout *layout,
                                        bContext *C,
                                        PointerRNA *ptr,
                                        blender::StringRefNull propname);

int uiTemplateRecentFiles(uiLayout *layout, int rows);
void uiTemplateFileSelectPath(uiLayout *layout, bContext *C, FileSelectParams *params);

namespace blender::ui {

void template_asset_shelf_popover(
    uiLayout &layout, const bContext &C, StringRefNull asset_shelf_id, StringRef name, int icon);

}

void uiTemplateLightLinkingCollection(uiLayout *layout,
                                      bContext *C,
                                      uiLayout *context_layout,
                                      PointerRNA *ptr,
                                      blender::StringRefNull propname);

void uiTemplateBoneCollectionTree(uiLayout *layout, bContext *C);
void uiTemplateGreasePencilLayerTree(uiLayout *layout, bContext *C);

void uiTemplateNodeTreeInterface(uiLayout *layout, const bContext *C, PointerRNA *ptr);
/**
 * Draw all node buttons and socket default values with the same panel structure used by the node.
 */
void uiTemplateNodeInputs(uiLayout *layout, bContext *C, PointerRNA *ptr);

void uiTemplateCollectionExporters(uiLayout *layout, bContext *C);

namespace blender::ed::object::shapekey {
void template_tree(uiLayout *layout, bContext *C);
}
/**
 * \return: True if the list item with unfiltered, unordered index \a item_idx is visible given the
 *          current filter settings.
 */
bool UI_list_item_index_is_filtered_visible(const struct uiList *ui_list, int item_idx);

/* UI Operators */
struct uiDragColorHandle {
  float color[4];
  bool gamma_corrected;
  bool has_alpha;
};

void ED_operatortypes_ui();
/**
 * \brief User Interface Keymap
 */
void ED_keymap_ui(wmKeyConfig *keyconf);
void ED_dropboxes_ui();
void ED_uilisttypes_ui();

void UI_drop_color_copy(bContext *C, wmDrag *drag, wmDropBox *drop);
bool UI_drop_color_poll(bContext *C, wmDrag *drag, const wmEvent *event);

bool UI_context_copy_to_selected_list(bContext *C,
                                      PointerRNA *ptr,
                                      PropertyRNA *prop,
                                      blender::Vector<PointerRNA> *r_lb,
                                      bool *r_use_path_from_id,
                                      std::optional<std::string> *r_path);
bool UI_context_copy_to_selected_check(PointerRNA *ptr,
                                       PointerRNA *ptr_link,
                                       PropertyRNA *prop,
                                       const char *path,
                                       bool use_path_from_id,
                                       PointerRNA *r_ptr,
                                       PropertyRNA **r_prop);

/* Helpers for Operators */
uiBut *UI_context_active_but_get(const bContext *C);
/**
 * Version of #UI_context_active_get() that uses the result of #CTX_wm_region_popup() if set.
 * Does not traverse into parent menus, which may be wanted in some cases.
 */
uiBut *UI_context_active_but_get_respect_popup(const bContext *C);
/**
 * Version of #UI_context_active_but_get that also returns RNA property info.
 * Helper function for insert keyframe, reset to default, etc operators.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
uiBut *UI_context_active_but_prop_get(const bContext *C,
                                      PointerRNA *r_ptr,
                                      PropertyRNA **r_prop,
                                      int *r_index);

/**
 * As above, but for a specified region.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
uiBut *UI_region_active_but_prop_get(const ARegion *region,
                                     PointerRNA *r_ptr,
                                     PropertyRNA **r_prop,
                                     int *r_index);

void UI_context_active_but_prop_handle(bContext *C, bool handle_undo);
void UI_context_active_but_clear(bContext *C, wmWindow *win, ARegion *region);

wmOperator *UI_context_active_operator_get(const bContext *C);
/**
 * Helper function for insert keyframe, reset to default, etc operators.
 */
void UI_context_update_anim_flag(const bContext *C);
void UI_context_active_but_prop_get_filebrowser(const bContext *C,
                                                PointerRNA *r_ptr,
                                                PropertyRNA **r_prop,
                                                bool *r_is_undo,
                                                bool *r_is_userdef);
/**
 * For new/open operators.
 *
 * This is for browsing and editing the ID-blocks used.
 */
void UI_context_active_but_prop_get_templateID(const bContext *C,
                                               PointerRNA *r_ptr,
                                               PropertyRNA **r_prop);
ID *UI_context_active_but_get_tab_ID(bContext *C);

uiBut *UI_region_active_but_get(const ARegion *region);
uiBut *UI_region_but_find_rect_over(const ARegion *region, const rcti *rect_px);
uiBlock *UI_region_block_find_mouse_over(const ARegion *region, const int xy[2], bool only_clip);
/**
 * Try to find a search-box region opened from a button in \a button_region.
 */
ARegion *UI_region_searchbox_region_get(const ARegion *button_region);

/** #uiFontStyle.align */
enum eFontStyle_Align {
  UI_STYLE_TEXT_LEFT = 0,
  UI_STYLE_TEXT_CENTER = 1,
  UI_STYLE_TEXT_RIGHT = 2,
};

struct uiFontStyleDraw_Params {
  eFontStyle_Align align;
  uint word_wrap : 1;
};

/* Styled text draw */
void UI_fontstyle_set(const uiFontStyle *fs);
void UI_fontstyle_draw_ex(const uiFontStyle *fs,
                          const rcti *rect,
                          const char *str,
                          size_t str_len,
                          const uchar col[4],
                          const uiFontStyleDraw_Params *fs_params,
                          int *r_xofs,
                          int *r_yofs,
                          ResultBLF *r_info);

void UI_fontstyle_draw(const uiFontStyle *fs,
                       const rcti *rect,
                       const char *str,
                       size_t str_len,
                       const uchar col[4],
                       const uiFontStyleDraw_Params *fs_params);

void UI_fontstyle_draw_multiline_clipped_ex(const uiFontStyle *fs,
                                            const rcti *rect,
                                            const char *str,
                                            const uchar col[4],
                                            eFontStyle_Align align,
                                            int *r_xofs,
                                            int *r_yofs,
                                            ResultBLF *r_info);
/**
 * Draws text with wrapping and shortening using "..." so that it fits into the given rectangle.
 */
void UI_fontstyle_draw_multiline_clipped(const uiFontStyle *fs,
                                         const rcti *rect,
                                         const char *str,
                                         const uchar col[4],
                                         eFontStyle_Align align);

/**
 * Drawn same as #UI_fontstyle_draw, but at 90 degree angle.
 */
void UI_fontstyle_draw_rotated(const uiFontStyle *fs,
                               const rcti *rect,
                               const char *str,
                               const uchar col[4]);
/**
 * Similar to #UI_fontstyle_draw
 * but ignore alignment, shadow & no clipping rect.
 *
 * For drawing on-screen labels.
 */
void UI_fontstyle_draw_simple(
    const uiFontStyle *fs, float x, float y, const char *str, const uchar col[4]);
/**
 * Same as #UI_fontstyle_draw but draw a colored backdrop.
 */
void UI_fontstyle_draw_simple_backdrop(const uiFontStyle *fs,
                                       float x,
                                       float y,
                                       blender::StringRef str,
                                       const float col_fg[4],
                                       const float col_bg[4]);

int UI_fontstyle_string_width(const uiFontStyle *fs, const char *str) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
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
int UI_fontstyle_string_width_with_block_aspect(const uiFontStyle *fs,
                                                blender::StringRef str,
                                                float aspect) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int UI_fontstyle_height_max(const uiFontStyle *fs);

/**
 * Triangle 'icon' for panel header and other cases.
 */
void UI_draw_icon_tri(float x, float y, char dir, const float[4]);

/**
 * Read a style (without any scaling applied).
 */
const uiStyle *UI_style_get(); /* use for fonts etc */
/**
 * Read a style (with the current DPI applied).
 */
const uiStyle *UI_style_get_dpi();

/* #UI_OT_editsource helpers. */
bool UI_editsource_enable_check();
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
void UI_but_ensure_in_view(const bContext *C, ARegion *region, const uiBut *but);

/* UI_butstore_ helpers */
struct uiButStore;

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
void UI_butstore_free(uiBlock *block, uiButStore *bs_handle);
bool UI_butstore_is_valid(uiButStore *bs_handle);
bool UI_butstore_is_registered(uiBlock *block, uiBut *but);
void UI_butstore_register(uiButStore *bs_handle, uiBut **but_p);
/**
 * Update the pointer for a registered button.
 */
bool UI_butstore_register_update(uiBlock *block, uiBut *but_dst, const uiBut *but_src);
void UI_butstore_unregister(uiButStore *bs_handle, uiBut **but_p);

/* ui_interface_region_tooltip.c */

/**
 * \param is_quick_tip: See #UI_but_func_quick_tooltip_set for what a quick tooltip is.
 */
ARegion *UI_tooltip_create_from_button(bContext *C,
                                       ARegion *butregion,
                                       uiBut *but,
                                       bool is_quick_tip);
ARegion *UI_tooltip_create_from_button_or_extra_icon(
    bContext *C, ARegion *butregion, uiBut *but, uiButExtraOpIcon *extra_icon, bool is_quick_tip);
ARegion *UI_tooltip_create_from_gizmo(bContext *C, wmGizmo *gz);
void UI_tooltip_free(bContext *C, bScreen *screen, ARegion *region);

/**
 * Create a tooltip from search-item tooltip data \a item_tooltip data.
 * To be called from a callback set with #UI_but_func_search_set_tooltip().
 *
 * \param item_rect: Rectangle of the search item in search region space (#ui_searchbox_butrect())
 *                   which is passed to the tooltip callback.
 */
ARegion *UI_tooltip_create_from_search_item_generic(bContext *C,
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
#define UI_FSTYLE_WIDGET (const uiFontStyle *)&(UI_style_get()->widget)
#define UI_FSTYLE_TOOLTIP (const uiFontStyle *)&(UI_style_get()->tooltip)

/**
 * Returns the best "UI" precision for given floating value,
 * so that e.g. 10.000001 rather gets drawn as '10'...
 */
int UI_calc_float_precision(int prec, double value);

/* widget batched drawing */
void UI_widgetbase_draw_cache_begin();
void UI_widgetbase_draw_cache_flush();
void UI_widgetbase_draw_cache_end();

/* Use for resetting the theme. */
/**
 * Initialize default theme.
 *
 * \note When you add new colors, created & saved themes need initialized
 * use function below, #init_userdef_do_versions.
 */
void UI_theme_init_default();
void UI_style_init_default();

void UI_interface_tag_script_reload();

/** Special drawing for toolbar, mainly workarounds for inflexible icon sizing. */
#define USE_UI_TOOLBAR_HACK

/** Support click-drag motion which presses the button and closes a popover (like a menu). */
#define USE_UI_POPOVER_ONCE

bool UI_view_item_matches(const blender::ui::AbstractViewItem &a,
                          const blender::ui::AbstractViewItem &b);
/**
 * Can \a item be renamed right now? Note that this isn't just a mere wrapper around
 * #AbstractViewItem::supports_renaming(). This also checks if there is another item being renamed,
 * and returns false if so.
 */
bool UI_view_item_can_rename(const blender::ui::AbstractViewItem &item);
void UI_view_item_begin_rename(blender::ui::AbstractViewItem &item);

bool UI_view_item_supports_drag(const blender::ui::AbstractViewItem &item);
/** If this view is displayed in a popup, don't close it when clicking to activate items. */
bool UI_view_item_popup_keep_open(const blender::ui::AbstractViewItem &item);
/**
 * Attempt to start dragging \a item_. This will not work if the view item doesn't
 * support dragging, i.e. if it won't create a drag-controller upon request.
 * \return True if dragging started successfully, otherwise false.
 */
bool UI_view_item_drag_start(bContext &C, blender::ui::AbstractViewItem &item);

/**
 * \param xy: Coordinate to find a view item at, in window space.
 * \param pad: Extra padding added to the bounding box of the view.
 */
blender::ui::AbstractView *UI_region_view_find_at(const ARegion *region, const int xy[2], int pad);
/**
 * \param xy: Coordinate to find a view item at, in window space.
 */
blender::ui::AbstractViewItem *UI_region_views_find_item_at(const ARegion &region,
                                                            const int xy[2]);
blender::ui::AbstractViewItem *UI_region_views_find_active_item(const ARegion *region);
uiBut *UI_region_views_find_active_item_but(const ARegion *region);
void UI_region_views_clear_search_highlight(const ARegion *region);
