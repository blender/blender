/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#pragma once

#include <functional>

#include "BLI_compiler_attrs.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_fcurve.hh"

#include "DNA_listBase.h"
#include "RNA_types.hh"
#include "UI_interface.hh"
#include "UI_resources.hh"

struct AnimationEvalContext;
struct ARegion;
struct bContext;
struct bContextStore;
struct ColorManagedDisplay;
struct CurveMapping;
struct CurveProfile;
namespace blender::gpu {
class Batch;
}
struct IconTextOverlay;
struct ID;
struct ImBuf;
struct Main;
struct Scene;
struct uiHandleButtonData;
struct uiLayout;
struct uiListType;
struct uiStyle;
struct uiUndoStack_Text;
struct uiWidgetColors;
struct UnitSettings;
struct wmEvent;
struct wmKeyConfig;
struct wmOperatorType;
struct wmTimer;

/* ****************** general defines ************** */

#define RNA_NO_INDEX -1
#define RNA_ENUM_VALUE -2

#define UI_MENU_PADDING (int)(0.2f * UI_UNIT_Y)

#define UI_MENU_WIDTH_MIN (UI_UNIT_Y * 9)
/** Some extra padding added to menus containing sub-menu icons. */
#define UI_MENU_SUBMENU_PADDING (6 * UI_SCALE_FAC)

/* menu scrolling */
#define UI_MENU_SCROLL_ARROW (12 * UI_SCALE_FAC)
#define UI_MENU_SCROLL_MOUSE (UI_MENU_SCROLL_ARROW + 2 * UI_SCALE_FAC)
#define UI_MENU_SCROLL_PAD (4 * UI_SCALE_FAC)

/** Popover width (multiplied by #U.widget_unit) */
#define UI_POPOVER_WIDTH_UNITS 10

/** #uiBut.flag */
enum {
  /** Use when the button is pressed. */
  UI_SELECT = (1 << 0),
  /** Temporarily hidden (scrolled out of the view). */
  UI_SCROLLED = (1 << 1),
  /**
   * The button is hovered by the mouse and should be drawn with a hover highlight. Also set
   * sometimes to highlight buttons without actually hovering it (e.g. for arrow navigation in
   * menus). UI handling code manages this mostly and usually does this together with making the
   * button active/focused (see #uiBut::active). This means events will be forwarded to it and
   * further handlers/shortcuts can be used while hovering it.
   */
  UI_HOVER = (1 << 2),
  UI_HAS_ICON = (1 << 3),
  UI_HIDDEN = (1 << 4),
  /** Display selected, doesn't impact interaction. */
  UI_SELECT_DRAW = (1 << 5),
  /** Property search filter is active and the button does not match. */
  UI_SEARCH_FILTER_NO_MATCH = (1 << 6),

  /** Temporarily override the active button for lookups in context, regions, etc. (everything
   * using #ui_context_button_active()). For example, so that operators normally acting on the
   * active button can be polled on non-active buttons to (e.g. for disabling). */
  UI_BUT_ACTIVE_OVERRIDE = (1 << 7),

  /* WARNING: rest of #uiBut.flag in UI_interface.hh */
};

/** #uiBut.pie_dir */
enum RadialDirection {
  UI_RADIAL_NONE = -1,
  UI_RADIAL_N = 0,
  UI_RADIAL_NE = 1,
  UI_RADIAL_E = 2,
  UI_RADIAL_SE = 3,
  UI_RADIAL_S = 4,
  UI_RADIAL_SW = 5,
  UI_RADIAL_W = 6,
  UI_RADIAL_NW = 7,
};

/** Next direction (clockwise). */
#define UI_RADIAL_DIRECTION_NEXT(dir) RadialDirection((int(dir) + 1) % (int(UI_RADIAL_NW) + 1))
/** Previous direction (counter-clockwise). */
#define UI_RADIAL_DIRECTION_PREV(dir) \
  RadialDirection(((int(dir) + int(UI_RADIAL_NW))) % (int(UI_RADIAL_NW) + 1))

/** Store a mask for diagonal directions. */
#define UI_RADIAL_MASK_ALL_DIAGONAL \
  ((1 << int(UI_RADIAL_NE)) | (1 << int(UI_RADIAL_SE)) | (1 << int(UI_RADIAL_SW)) | \
   (1 << int(UI_RADIAL_NW)))
#define UI_RADIAL_MASK_ALL_AXIS_ALIGNED \
  ((1 << int(UI_RADIAL_N)) | (1 << int(UI_RADIAL_S)) | (1 << int(UI_RADIAL_E)) | \
   (1 << int(UI_RADIAL_W)))

extern const char ui_radial_dir_order[8];
extern const char ui_radial_dir_to_numpad[8];
extern const short ui_radial_dir_to_angle[8];

/* internal panel drawing defines */
#define PNL_HEADER (UI_UNIT_Y * 1.25) /* 24 default */

/* bit button defines */
/* Bit operations */
#define UI_BITBUT_TEST(a, b) (((a) & (1 << (b))) != 0)
#define UI_BITBUT_VALUE_TOGGLED(a, b) ((a) ^ (1 << (b)))
#define UI_BITBUT_VALUE_ENABLED(a, b) ((a) | (1 << (b)))
#define UI_BITBUT_VALUE_DISABLED(a, b) ((a) & ~(1 << (b)))

/* bit-row */
#define UI_BITBUT_ROW(min, max) \
  (((max) >= 31 ? 0xFFFFFFFF : (1 << ((max) + 1)) - 1) - ((min) ? ((1 << (min)) - 1) : 0))

/** Split number-buttons by ':' and align left/right. */
#define USE_NUMBUTS_LR_ALIGN

/** Use new 'align' computation code. */
#define USE_UIBUT_SPATIAL_ALIGN

/** #PieMenuData.flags */
enum {
  /** Use initial center of pie menu to calculate direction. */
  UI_PIE_INITIAL_DIRECTION = (1 << 1),
  /** Pie menu is drag style. */
  UI_PIE_DRAG_STYLE = (1 << 2),
  /** Mouse not far enough from center position. */
  UI_PIE_INVALID_DIR = (1 << 3),
  /** Pie menu changed to click style, click to confirm. */
  UI_PIE_CLICK_STYLE = (1 << 4),
  /** Pie animation finished, do not calculate any more motion. */
  UI_PIE_ANIMATION_FINISHED = (1 << 5),
  /** Pie gesture selection has been done, now wait for mouse motion to end. */
  UI_PIE_GESTURE_END_WAIT = (1 << 6),
};

#define PIE_CLICK_THRESHOLD_SQ 50.0f

/** The maximum number of items a radial menu (pie menu) can contain. */
#define PIE_MAX_ITEMS 8

struct uiBut {
  uiBut *next = nullptr, *prev = nullptr;

  /** Pointer back to the layout item holding this button. */
  uiLayout *layout = nullptr;
  int flag = 0;
  int flag2 = 0;
  int drawflag = 0;
  eButType type = eButType(0);
  eButPointerType pointype = UI_BUT_POIN_NONE;
  short bit = 0, bitnr = 0, retval = 0, strwidth = 0, alignnr = 0;
  short ofs = 0, pos = 0, selsta = 0, selend = 0;

  std::string str;

  std::string drawstr;

  char *placeholder = nullptr;

  rctf rect = {}; /* block relative coords */

  char *poin = nullptr;
  float hardmin = 0, hardmax = 0, softmin = 0, softmax = 0;

  uchar col[4] = {0};

  /** See \ref UI_but_func_identity_compare_set(). */
  uiButIdentityCompareFunc identity_cmp_func = nullptr;

  uiButHandleFunc func = nullptr;
  void *func_arg1 = nullptr;
  void *func_arg2 = nullptr;
  /**
   * C++ version of #func above. Allows storing arbitrary data in a type safe way, no void
   * pointer arguments.
   */
  std::function<void(bContext &)> apply_func;

  uiButHandleNFunc funcN = nullptr;
  void *func_argN = nullptr;

  const bContextStore *context = nullptr;

  uiButCompleteFunc autocomplete_func = nullptr;
  void *autofunc_arg = nullptr;

  uiButHandleRenameFunc rename_func = nullptr;
  void *rename_arg1 = nullptr;
  void *rename_orig = nullptr;

  /** Run an action when holding the button down. */
  uiButHandleHoldFunc hold_func = nullptr;
  void *hold_argN = nullptr;

  const char *tip = nullptr;
  uiButToolTipFunc tip_func = nullptr;
  void *tip_arg = nullptr;
  uiFreeArgFunc tip_arg_free = nullptr;
  /** Function to override the label to be displayed in the tooltip. */
  std::function<std::string(const uiBut *)> tip_label_func;

  uiButToolTipCustomFunc tip_custom_func = nullptr;

  /** info on why button is disabled, displayed in tooltip */
  const char *disabled_info = nullptr;

  BIFIconID icon = ICON_NONE;
  /** Copied from the #uiBlock.emboss */
  eUIEmbossType emboss = UI_EMBOSS;
  /** direction in a pie menu, used for collision detection. */
  RadialDirection pie_dir = UI_RADIAL_NONE;
  /** could be made into a single flag */
  bool changed = false;
  /** so buttons can support unit systems which are not RNA */
  uchar unit_type = 0;
  short iconadd = 0;

  /** Affects the order if this uiBut is used in menu-search. */
  float search_weight = 0.0f;

  /** #UI_BTYPE_BLOCK data */
  uiBlockCreateFunc block_create_func = nullptr;

  /** #UI_BTYPE_PULLDOWN / #UI_BTYPE_MENU data */
  uiMenuCreateFunc menu_create_func = nullptr;

  uiMenuStepFunc menu_step_func = nullptr;

  /* RNA data */
  PointerRNA rnapoin = {};
  PropertyRNA *rnaprop = nullptr;
  int rnaindex = 0;

  /* Operator data */
  wmOperatorType *optype = nullptr;
  PointerRNA *opptr = nullptr;
  wmOperatorCallContext opcontext = WM_OP_INVOKE_DEFAULT;

  /** When non-zero, this is the key used to activate a menu items (`a-z` always lower case). */
  uchar menu_key = 0;

  ListBase extra_op_icons = {nullptr, nullptr}; /** #uiButExtraOpIcon */

  eWM_DragDataType dragtype = WM_DRAG_ID;
  short dragflag = 0;
  void *dragpoin = nullptr;
  const ImBuf *imb = nullptr;
  float imb_scale = 0;

  /**
   * Active button data, set when the user is hovering or interacting with a button (#UI_HOVER and
   * #UI_SELECT state mostly).
   */
  uiHandleButtonData *active = nullptr;

  /** Custom button data (borrowed, not owned). */
  void *custom_data = nullptr;

  char *editstr = nullptr;
  double *editval = nullptr;
  float *editvec = nullptr;

  std::function<bool(const uiBut &)> pushed_state_func;

  /** Little indicator (e.g., counter) displayed on top of some icons. */
  IconTextOverlay icon_overlay_text = {};

  /* pointer back */
  uiBlock *block = nullptr;

  uiBut() = default;
  /** Performs a mostly shallow copy for now. Only contained C++ types are deep copied. */
  uiBut(const uiBut &other) = default;
  /** Mostly shallow copy, just like copy constructor above. */
  uiBut &operator=(const uiBut &other) = default;
};

/** Derived struct for #UI_BTYPE_NUM */
struct uiButNumber : public uiBut {
  float step_size = 0.0f;
  float precision = 0.0f;
};

/** Derived struct for #UI_BTYPE_NUM_SLIDER */
struct uiButNumberSlider : public uiBut {
  float step_size = 0.0f;
  float precision = 0.0f;
};

/** Derived struct for #UI_BTYPE_COLOR */
struct uiButColor : public uiBut {
  bool is_pallete_color = false;
  int palette_color_index = -1;
};

/** Derived struct for #UI_BTYPE_TAB */
struct uiButTab : public uiBut {
  MenuType *menu = nullptr;
};

/** Derived struct for #UI_BTYPE_SEARCH_MENU */
struct uiButSearch : public uiBut {
  uiButSearchCreateFn popup_create_fn = nullptr;
  uiButSearchUpdateFn items_update_fn = nullptr;
  uiButSearchListenFn listen_fn = nullptr;

  void *item_active = nullptr;
  char *item_active_str;

  void *arg = nullptr;
  uiFreeArgFunc arg_free_fn = nullptr;

  uiButSearchContextMenuFn item_context_menu_fn = nullptr;
  uiButSearchTooltipFn item_tooltip_fn = nullptr;

  const char *item_sep_string = nullptr;

  PointerRNA rnasearchpoin = {};
  PropertyRNA *rnasearchprop = nullptr;

  int preview_rows = 0;
  int preview_cols = 0;

  /**
   * The search box only provides suggestions, it does not force
   * the string to match one of the search items when applying.
   */
  bool results_are_suggestions = false;
};

/**
 * Derived struct for #UI_BTYPE_DECORATOR
 * Decorators have their own RNA data, using the normal #uiBut RNA members has many side-effects.
 */
struct uiButDecorator : public uiBut {
  struct PointerRNA decorated_rnapoin = {};
  struct PropertyRNA *decorated_rnaprop = nullptr;
  int decorated_rnaindex = -1;
};

/** Derived struct for #UI_BTYPE_PROGRESS. */
struct uiButProgress : public uiBut {
  /** Progress in  0..1 range */
  float progress_factor = 0.0f;
  /** The display style (bar, pie... etc). */
  eButProgressType progress_type = UI_BUT_PROGRESS_TYPE_BAR;
};

/** Derived struct for #UI_BTYPE_SEPR_LINE. */
struct uiButSeparatorLine : public uiBut {
  bool is_vertical;
};

/** Derived struct for #UI_BTYPE_LABEL. */
struct uiButLabel : public uiBut {
  float alpha_factor = 1.0f;
};

/** Derived struct for #UI_BTYPE_SCROLL. */
struct uiButScrollBar : public uiBut {
  /** Actual visual height of UI list (in rows). */
  float visual_height = -1.0f;
};

struct uiButViewItem : public uiBut {
  /* The view item this button was created for. */
  blender::ui::AbstractViewItem *view_item = nullptr;
  /* Some items want to have a fixed size for drawing, differing from the interaction rectangle
   * (e.g. so highlights are drawn smaller). */
  int draw_width = 0;
  int draw_height = 0;
};

/** Derived struct for #UI_BTYPE_HSVCUBE. */
struct uiButHSVCube : public uiBut {
  eButGradientType gradient_type = UI_GRAD_SV;
};

/** Derived struct for #UI_BTYPE_COLORBAND. */
struct uiButColorBand : public uiBut {
  ColorBand *edit_coba = nullptr;
};

/** Derived struct for #UI_BTYPE_CURVEPROFILE. */
struct uiButCurveProfile : public uiBut {
  CurveProfile *edit_profile = nullptr;
};

/** Derived struct for #UI_BTYPE_CURVE. */
struct uiButCurveMapping : public uiBut {
  CurveMapping *edit_cumap = nullptr;
  eButGradientType gradient_type = UI_GRAD_SV;
};

/** Derived struct for #UI_BTYPE_HOTKEY_EVENT. */
struct uiButHotkeyEvent : public uiBut {
  short modifier_key = 0;
};

/**
 * Additional, superimposed icon for a button, invoking an operator.
 */
struct uiButExtraOpIcon {
  uiButExtraOpIcon *next, *prev;

  BIFIconID icon;
  wmOperatorCallParams *optype_params;

  bool highlighted;
  bool disabled;
};

struct ColorPicker {
  ColorPicker *next, *prev;

  /** Color in HSV or HSL, in color picking color space. Used for HSV cube,
   * circle and slider widgets. The color picking space is perceptually
   * linear for intuitive editing. */
  float hsv_perceptual[3];
  /** Initial color data (to detect changes). */
  float hsv_perceptual_init[3];
  bool is_init;

  /** HSV or HSL color in scene linear color space value used for number
   * buttons. This is scene linear so that there is a clear correspondence
   * to the scene linear RGB values. */
  float hsv_scene_linear[3];

  /** Cubic saturation for the color wheel. */
  bool use_color_cubic;
  bool use_color_lock;
  bool use_luminosity_lock;
  float luminosity_lock_value;
};

struct ColorPickerData {
  ListBase list;
};

struct PieMenuData {
  /** store title and icon to allow access when pie levels are created */
  const char *title;
  int icon;

  /** A mask combining the directions of all buttons in the pie menu (excluding separators). */
  int pie_dir_mask;
  float pie_dir[2];
  float pie_center_init[2];
  float pie_center_spawned[2];
  float last_pos[2];
  double duration_gesture;
  int flags;
  /** Initial event used to fire the pie menu, store here so we can query for release */
  short event_type;
  float alphafac;
};

/** #uiBlock.content_hints */
enum eBlockContentHints {
  /** In a menu block, if there is a single sub-menu button, we add some
   * padding to the right to put nicely aligned triangle icons there. */
  UI_BLOCK_CONTAINS_SUBMENU_BUT = (1 << 0),
};

/* #uiButtonGroup.flag. */
enum uiButtonGroupFlag {
  /** While this flag is set, don't create new button groups for layout item calls. */
  UI_BUTTON_GROUP_LOCK = (1 << 0),
  /** The buttons in this group are inside a panel header. */
  UI_BUTTON_GROUP_PANEL_HEADER = (1 << 1),
};
ENUM_OPERATORS(uiButtonGroupFlag, UI_BUTTON_GROUP_PANEL_HEADER);

/**
 * A group of button references, used by property search to keep track of sets of buttons that
 * should be searched together. For example, in property split layouts number buttons and their
 * labels (and even their decorators) are separate buttons, but they must be searched and
 * highlighted together.
 */
struct uiButtonGroup {
  blender::Vector<uiBut *> buttons;
  uiButtonGroupFlag flag;
};

struct uiBlockDynamicListener {
  uiBlockDynamicListener *next, *prev;

  void (*listener_func)(const wmRegionListenerParams *params);
};

struct uiBlock {
  uiBlock *next, *prev;

  ListBase buttons;
  Panel *panel;
  uiBlock *oldblock;

  /** Used for `UI_butstore_*` runtime function. */
  ListBase butstore;

  blender::Vector<uiButtonGroup> button_groups;

  ListBase layouts;
  uiLayout *curlayout;

  blender::Vector<std::unique_ptr<bContextStore>> contexts;

  /** A block can store "views" on data-sets. Currently tree-views (#AbstractTreeView) only.
   * Others are imaginable, e.g. table-views, grid-views, etc. These are stored here to support
   * state that is persistent over redraws (e.g. collapsed tree-view items). */
  ListBase views;

  ListBase dynamic_listeners; /* #uiBlockDynamicListener */

  std::string name;

  float winmat[4][4];

  rctf rect;
  float aspect;

  /** Unique hash used to implement popup menu memory. */
  uint puphash;

  uiButHandleFunc func;
  void *func_arg1;
  void *func_arg2;

  uiButHandleNFunc funcN;
  void *func_argN;

  uiBlockHandleFunc handle_func;
  void *handle_func_arg;

  /** Custom interaction data. */
  uiBlockInteraction_CallbackData custom_interaction_callbacks;

  /** Custom extra event handling. */
  int (*block_event_func)(const bContext *C, uiBlock *, const wmEvent *);

  /** Custom extra draw function for custom blocks. */
  void (*drawextra)(const bContext *C, void *idv, void *arg1, void *arg2, rcti *rect);
  void *drawextra_arg1;
  void *drawextra_arg2;

  int flag;
  short alignnr;
  /** Hints about the buttons of this block. Used to avoid iterating over
   * buttons to find out if some criteria is met by any. Instead, check this
   * criteria when adding the button and set a flag here if it's met. */
  short content_hints; /* #eBlockContentHints */

  char direction;
  /** UI_BLOCK_THEME_STYLE_* */
  char theme_style;
  /** Copied to #uiBut.emboss */
  eUIEmbossType emboss;
  bool auto_open;
  char _pad[5];
  double auto_open_last;

  const char *lockstr;

  bool lock;
  /** To keep blocks while drawing and free them afterwards. */
  bool active;
  /** To avoid tool-tip after click. */
  bool tooltipdisabled;
  /** True when #UI_block_end has been called. */
  bool endblock;

  /** for doing delayed */
  eBlockBoundsCalc bounds_type;
  /** Offset to use when calculating bounds (in pixels). */
  int bounds_offset[2];
  /** for doing delayed */
  int bounds, minbounds;

  /** Pull-downs, to detect outside, can differ per case how it is created. */
  rctf safety;
  /** #uiSafetyRct list */
  ListBase saferct;

  uiPopupBlockHandle *handle;

  /** use so presets can find the operator,
   * across menus and from nested popups which fail for operator context. */
  wmOperator *ui_operator;
  bool ui_operator_free;

  /** XXX hack for dynamic operator enums */
  void *evil_C;

  /** unit system, used a lot for numeric buttons so include here
   * rather than fetching through the scene every time. */
  const UnitSettings *unit;
  /** \note only accessed by color picker templates. */
  ColorPickerData color_pickers;

  /** Block for color picker with gamma baked in. */
  bool is_color_gamma_picker;

  /**
   * Display device name used to display this block,
   * used by color widgets to transform colors from/to scene linear.
   */
  char display_device[64];

  PieMenuData pie_data;
};

struct uiSafetyRct {
  uiSafetyRct *next, *prev;
  rctf parent;
  rctf safety;
};

/* `interface.cc` */

void ui_fontscale(float *points, float aspect);

/** Project button or block (but==nullptr) to pixels in region-space. */
void ui_but_to_pixelrect(rcti *rect,
                         const ARegion *region,
                         const uiBlock *block,
                         const uiBut *but);
rcti ui_to_pixelrect(const ARegion *region, const uiBlock *block, const rctf *src_rect);

void ui_block_to_region_fl(const ARegion *region, const uiBlock *block, float *r_x, float *r_y);
void ui_block_to_window_fl(const ARegion *region, const uiBlock *block, float *x, float *y);
void ui_block_to_window(const ARegion *region, const uiBlock *block, int *x, int *y);
void ui_block_to_region_rctf(const ARegion *region,
                             const uiBlock *block,
                             rctf *rct_dst,
                             const rctf *rct_src);
void ui_block_to_window_rctf(const ARegion *region,
                             const uiBlock *block,
                             rctf *rct_dst,
                             const rctf *rct_src);
float ui_block_to_window_scale(const ARegion *region, const uiBlock *block);
/**
 * For mouse cursor.
 */
void ui_window_to_block_fl(const ARegion *region, const uiBlock *block, float *x, float *y);
void ui_window_to_block(const ARegion *region, const uiBlock *block, int *x, int *y);
void ui_window_to_block_rctf(const ARegion *region,
                             const uiBlock *block,
                             rctf *rct_dst,
                             const rctf *rct_src);
void ui_window_to_region(const ARegion *region, int *x, int *y);
void ui_window_to_region_rcti(const ARegion *region, rcti *rect_dst, const rcti *rct_src);
void ui_window_to_region_rctf(const ARegion *region, rctf *rect_dst, const rctf *rct_src);
void ui_region_to_window(const ARegion *region, int *x, int *y);
/**
 * Popups will add a margin to #ARegion.winrct for shadow,
 * for interactivity (point-inside tests for eg), we want the winrct without the margin added.
 */
void ui_region_winrct_get_no_margin(const ARegion *region, rcti *r_rect);

/** Register a listener callback to this block to tag the area/region for redraw. */
void ui_block_add_dynamic_listener(uiBlock *block,
                                   void (*listener_func)(const wmRegionListenerParams *params));

/**
 * Reallocate the button (new address is returned) for a new button type.
 * This should generally be avoided and instead the correct type be created right away.
 *
 * \note Only the #uiBut data can be kept. If the old button used a derived type (e.g. #uiButTab),
 *       the data that is not inside #uiBut will be lost.
 */
uiBut *ui_but_change_type(uiBut *but, eButType new_type);

double ui_but_value_get(uiBut *but);
void ui_but_value_set(uiBut *but, double value);
/**
 * For picker, while editing HSV.
 */
void ui_but_hsv_set(uiBut *but);
/**
 * For buttons pointing to color for example.
 */
void ui_but_v3_get(uiBut *but, float vec[3]);
/**
 * For buttons pointing to color for example.
 */
void ui_but_v3_set(uiBut *but, const float vec[3]);

void ui_hsvcircle_vals_from_pos(
    const rcti *rect, float mx, float my, float *r_val_rad, float *r_val_dist);
/**
 * Cursor in HSV circle, in float units -1 to 1, to map on radius.
 */
void ui_hsvcircle_pos_from_vals(
    const ColorPicker *cpicker, const rcti *rect, const float *hsv, float *xpos, float *ypos);
void ui_hsvcube_pos_from_vals(
    const uiButHSVCube *hsv_but, const rcti *rect, const float *hsv, float *xp, float *yp);

/**
 * \param float_precision: For number buttons the precision
 * to use or -1 to fallback to the button default.
 * \param use_exp_float: Use exponent representation of floats
 * when out of reasonable range (outside of 1e3/1e-3).
 */
void ui_but_string_get_ex(uiBut *but,
                          char *str,
                          size_t str_maxncpy,
                          int float_precision,
                          bool use_exp_float,
                          bool *r_use_exp_float) ATTR_NONNULL(1, 2);
void ui_but_string_get(uiBut *but, char *str, size_t str_maxncpy) ATTR_NONNULL();
/**
 * A version of #ui_but_string_get_ex for dynamic buffer sizes
 * (where #ui_but_string_get_maxncpy returns 0).
 *
 * \param r_str_size: size of the returned string (including terminator).
 */
char *ui_but_string_get_dynamic(uiBut *but, int *r_str_size);
/**
 * \param str: will be overwritten.
 */
void ui_but_convert_to_unit_alt_name(uiBut *but, char *str, size_t str_maxncpy) ATTR_NONNULL();
bool ui_but_string_set(bContext *C, uiBut *but, const char *str) ATTR_NONNULL();
bool ui_but_string_eval_number(bContext *C, const uiBut *but, const char *str, double *value)
    ATTR_NONNULL();
int ui_but_string_get_maxncpy(uiBut *but);
/**
 * Clear & exit the active button's string..
 */
void ui_but_active_string_clear_and_exit(bContext *C, uiBut *but) ATTR_NONNULL();
/**
 * Use handling code to set a string for the button. Handles the case where the string is set for a
 * search button while the search menu is open, so the results are updated accordingly.
 * This is basically the same as pasting the string into the button.
 */
void ui_but_set_string_interactive(bContext *C, uiBut *but, const char *value);
uiBut *ui_but_drag_multi_edit_get(uiBut *but);

/**
 * Get the hint that describes the expected value when empty.
 */
const char *ui_but_placeholder_get(uiBut *but);

void ui_def_but_icon(uiBut *but, int icon, int flag);
/**
 * Avoid using this where possible since it's better not to ask for an icon in the first place.
 */
void ui_def_but_icon_clear(uiBut *but);

void ui_but_extra_operator_icons_free(uiBut *but);

void ui_but_rna_menu_convert_to_panel_type(uiBut *but, const char *panel_type);
void ui_but_rna_menu_convert_to_menu_type(uiBut *but, const char *menu_type);
bool ui_but_menu_draw_as_popover(const uiBut *but);

void ui_but_range_set_hard(uiBut *but);
void ui_but_range_set_soft(uiBut *but);

bool ui_but_context_poll_operator(bContext *C, wmOperatorType *ot, const uiBut *but);
/**
 * Check if the operator \a ot poll is successful with the context given by \a but (optionally).
 * \param but: The button that might store context. Can be NULL for convenience (e.g. if there is
 *             no button to take context from, but we still want to poll the operator).
 */
bool ui_but_context_poll_operator_ex(bContext *C,
                                     const uiBut *but,
                                     const wmOperatorCallParams *optype_params);

void ui_but_update(uiBut *but);
void ui_but_update_edited(uiBut *but);
PropertyScaleType ui_but_scale_type(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_float(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_bool(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_unit(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
/**
 * Check if this button is similar enough to be grouped with another.
 */
bool ui_but_is_compatible(const uiBut *but_a, const uiBut *but_b) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_rna_valid(uiBut *but) ATTR_WARN_UNUSED_RESULT;
/**
 * Checks if the button supports cycling next/previous menu items (ctrl+mouse-wheel).
 */
bool ui_but_supports_cycling(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

/**
 * Check if the button is pushed, this is only meaningful for some button types.
 *
 * \return (0 == UNSELECT), (1 == SELECT), (-1 == DO-NOTHING)
 */
int ui_but_is_pushed_ex(uiBut *but, double *value) ATTR_WARN_UNUSED_RESULT;
int ui_but_is_pushed(uiBut *but) ATTR_WARN_UNUSED_RESULT;

void ui_but_override_flag(Main *bmain, uiBut *but);

void ui_block_bounds_calc(uiBlock *block);

ColorManagedDisplay *ui_block_cm_display_get(uiBlock *block);
void ui_block_cm_to_display_space_v3(uiBlock *block, float pixel[3]);

/* `interface_regions.cc` */

struct uiKeyNavLock {
  /** Set when we're using keyboard-input. */
  bool is_keynav;
  /** Only used to check if we've moved the cursor. */
  int event_xy[2];
};

using uiBlockHandleCreateFunc = uiBlock *(*)(bContext *C, uiPopupBlockHandle *handle, void *arg1);

struct uiPopupBlockCreate {
  uiBlockCreateFunc create_func;
  uiBlockHandleCreateFunc handle_create_func;
  void *arg;
  uiFreeArgFunc arg_free;

  int event_xy[2];

  /** Set when popup is initialized from a button. */
  ARegion *butregion;
  uiBut *but;
};

struct uiPopupBlockHandle {
  /* internal */
  ARegion *region;

  /** Use only for #UI_BLOCK_MOVEMOUSE_QUIT popups. */
  float towards_xy[2];
  double towardstime;
  bool dotowards;

  bool popup;
  void (*popup_func)(bContext *C, void *arg, int event);
  void (*cancel_func)(bContext *C, void *arg);
  void *popup_arg;

  /** Store data for refreshing popups. */
  uiPopupBlockCreate popup_create_vars;
  /** True if we can re-create the popup using #uiPopupBlockHandle.popup_create_vars. */
  bool can_refresh;
  bool refresh;

  wmTimer *scrolltimer;
  float scrolloffset;

  uiKeyNavLock keynav_state;

  /* for operator popups */
  wmOperator *popup_op;
  ScrArea *ctx_area;
  ARegion *ctx_region;

  /* return values */
  int butretval;
  int menuretval;
  int retvalue;
  float retvec[4];

  /** Menu direction. */
  int direction;

  /* Previous values so we don't resize or reposition on refresh. */
  rctf prev_block_rect;
  rctf prev_butrct;
  short prev_dir1, prev_dir2;
  int prev_bounds_offset[2];

  /* Maximum estimated size to avoid having to reposition on refresh. */
  float max_size_x, max_size_y;

  /* #ifdef USE_DRAG_POPUP */
  bool is_grab;
  int grab_xy_prev[2];
  /* #endif */

  char menu_idname[64];
};

/* -------------------------------------------------------------------- */
/* interface_region_*.c */

/* `interface_region_tooltip.cc` */

/* exposed as public API in UI_interface.hh */

/* `interface_region_color_picker.cc` */

void ui_color_picker_rgb_to_hsv_compat(const float rgb[3], float r_cp[3]);
void ui_color_picker_rgb_to_hsv(const float rgb[3], float r_cp[3]);
void ui_color_picker_hsv_to_rgb(const float r_cp[3], float rgb[3]);

/**
 * Returns true if the button is for a color with gamma baked in,
 * or if it's a color picker for such a button.
 */
bool ui_but_is_color_gamma(uiBut *but);

void ui_scene_linear_to_perceptual_space(uiBut *but, float rgb[3]);
void ui_perceptual_to_scene_linear_space(uiBut *but, float rgb[3]);

uiBlock *ui_block_func_COLOR(bContext *C, uiPopupBlockHandle *handle, void *arg_but);
ColorPicker *ui_block_colorpicker_create(uiBlock *block);

/* `interface_region_search.cc` */

/**
 * Search-box for string button.
 */
ARegion *ui_searchbox_create_generic(bContext *C, ARegion *butregion, uiButSearch *search_but);
ARegion *ui_searchbox_create_operator(bContext *C, ARegion *butregion, uiButSearch *search_but);
ARegion *ui_searchbox_create_menu(bContext *C, ARegion *butregion, uiButSearch *search_but);

/**
 * x and y in screen-coords.
 */
bool ui_searchbox_inside(ARegion *region, const int xy[2]) ATTR_NONNULL(1, 2);
int ui_searchbox_find_index(ARegion *region, const char *name);
/**
 * Region is the search box itself.
 */
void ui_searchbox_update(bContext *C, ARegion *region, uiBut *but, bool reset);
int ui_searchbox_autocomplete(bContext *C, ARegion *region, uiBut *but, char *str);
bool ui_searchbox_event(
    bContext *C, ARegion *region, uiBut *but, ARegion *butregion, const wmEvent *event);
/**
 * String validated to be of correct length (but->hardmax).
 */
bool ui_searchbox_apply(uiBut *but, ARegion *region);
void ui_searchbox_free(bContext *C, ARegion *region);
/**
 * XXX weak: search_func adds all partial matches.
 */
void ui_but_search_refresh(uiButSearch *but);

/* `interface_region_menu_popup.cc` */

int ui_but_menu_step(uiBut *but, int direction);
bool ui_but_menu_step_poll(const uiBut *but);
uiBut *ui_popup_menu_memory_get(uiBlock *block);
void ui_popup_menu_memory_set(uiBlock *block, uiBut *but);

/**
 * Called for creating new popups and refreshing existing ones.
 */
uiBlock *ui_popup_block_refresh(bContext *C,
                                uiPopupBlockHandle *handle,
                                ARegion *butregion,
                                uiBut *but);

uiPopupBlockHandle *ui_popup_block_create(bContext *C,
                                          ARegion *butregion,
                                          uiBut *but,
                                          uiBlockCreateFunc create_func,
                                          uiBlockHandleCreateFunc handle_create_func,
                                          void *arg,
                                          uiFreeArgFunc arg_free);
uiPopupBlockHandle *ui_popup_menu_create(
    bContext *C, ARegion *butregion, uiBut *but, uiMenuCreateFunc menu_func, void *arg);

/* `interface_region_popover.cc` */

uiPopupBlockHandle *ui_popover_panel_create(bContext *C,
                                            ARegion *butregion,
                                            uiBut *but,
                                            uiMenuCreateFunc menu_func,
                                            const PanelType *panel_type);

/* `interface_region_menu_pie.cc` */

/**
 * Set up data for defining a new pie menu level and add button that invokes it.
 */
void ui_pie_menu_level_create(uiBlock *block,
                              wmOperatorType *ot,
                              const char *propname,
                              IDProperty *properties,
                              const EnumPropertyItem *items,
                              int totitem,
                              wmOperatorCallContext context,
                              eUI_Item_Flag flag);

/* `interface_region_popup.cc` */

/**
 * Translate any popup regions (so we can drag them).
 */
void ui_popup_translate(ARegion *region, const int mdiff[2]);
void ui_popup_block_free(bContext *C, uiPopupBlockHandle *handle);
void ui_popup_block_scrolltest(uiBlock *block);

/* end interface_region_*.c */

/* `interface_panel.cc` */

/**
 * Handle region panel events like opening and closing panels, changing categories, etc.
 *
 * \note Could become a modal key-map.
 */
int ui_handler_panel_region(bContext *C,
                            const wmEvent *event,
                            ARegion *region,
                            const uiBut *active_but);
/**
 * Draw a panel integrated in buttons-window, tool/property lists etc.
 */
void ui_draw_aligned_panel(const ARegion *region,
                           const uiStyle *style,
                           const uiBlock *block,
                           const rcti *rect,
                           bool show_pin,
                           bool show_background,
                           bool region_search_filter_active);
void ui_panel_tag_search_filter_match(Panel *panel);

/**
 * Draws in resolution of 48x4 colors.
 */
void ui_draw_gradient(const rcti *rect, const float hsv[3], eButGradientType type, float alpha);

/**
 * Draws rounded corner segments but inverted. Imagine each corner like a filled right triangle,
 * just that the hypotenuse is nicely curved inwards (towards the right angle of the triangle).
 *
 * Useful for connecting orthogonal shapes with a rounded corner, which can look quite nice.
 */
void ui_draw_rounded_corners_inverted(const rcti &rect,
                                      const float rad,
                                      const blender::float4 color);

/* based on UI_draw_roundbox_gl_mode,
 * check on making a version which allows us to skip some sides */
void ui_draw_but_TAB_outline(const rcti *rect,
                             float rad,
                             uchar highlight[3],
                             uchar highlight_fade[3]);
void ui_draw_but_HISTOGRAM(ARegion *region,
                           uiBut *but,
                           const uiWidgetColors *wcol,
                           const rcti *rect);
void ui_draw_but_WAVEFORM(ARegion *region,
                          uiBut *but,
                          const uiWidgetColors *wcol,
                          const rcti *rect);
void ui_draw_but_VECTORSCOPE(ARegion *region,
                             uiBut *but,
                             const uiWidgetColors *wcol,
                             const rcti *rect);
void ui_draw_but_COLORBAND(uiBut *but, const uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_UNITVEC(uiBut *but, const uiWidgetColors *wcol, const rcti *rect, float radius);
void ui_draw_but_CURVE(ARegion *region, uiBut *but, const uiWidgetColors *wcol, const rcti *rect);
/**
 * Draws the curve profile widget. Somewhat similar to ui_draw_but_CURVE.
 */
void ui_draw_but_CURVEPROFILE(ARegion *region,
                              uiBut *but,
                              const uiWidgetColors *wcol,
                              const rcti *rect);
void ui_draw_but_IMAGE(ARegion *region, uiBut *but, const uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_TRACKPREVIEW(ARegion *region,
                              uiBut *but,
                              const uiWidgetColors *wcol,
                              const rcti *rect);

/* `interface_undo.cc` */

/**
 * Start the undo stack.
 *
 * \note The current state should be pushed immediately after calling this.
 */
uiUndoStack_Text *ui_textedit_undo_stack_create();
void ui_textedit_undo_stack_destroy(uiUndoStack_Text *undo_stack);
/**
 * Push the information in the arguments to a new state in the undo stack.
 *
 * \note Currently the total length of the undo stack is not limited.
 */
void ui_textedit_undo_push(uiUndoStack_Text *undo_stack, const char *text, int cursor_index);
const char *ui_textedit_undo(uiUndoStack_Text *undo_stack, int direction, int *r_cursor_index);

/* interface_handlers.cc */

void ui_handle_afterfunc_add_operator(wmOperatorType *ot, wmOperatorCallContext opcontext);
/**
 * Assumes event type is MOUSEPAN.
 */
void ui_pan_to_scroll(const wmEvent *event, int *type, int *val);
/**
 * Exported to `interface.cc`: #UI_but_active_only()
 * \note The region is only for the button.
 * The context needs to be set by the caller.
 */
void ui_but_activate_event(bContext *C, ARegion *region, uiBut *but);
/**
 * Simulate moving the mouse over a button (or navigating to it with arrow keys).
 *
 * exported so menus can start with a highlighted button,
 * even if the mouse isn't over it
 */
void ui_but_activate_over(bContext *C, ARegion *region, uiBut *but);
void ui_but_execute_begin(bContext *C, ARegion *region, uiBut *but, void **active_back);
void ui_but_execute_end(bContext *C, ARegion *region, uiBut *but, void *active_back);
void ui_but_active_free(const bContext *C, uiBut *but);
/**
 * In some cases we may want to update the view (#View2D) in-between layout definition and drawing.
 * E.g. to make sure a button is visible while editing.
 */
void ui_but_update_view_for_active(const bContext *C, const uiBlock *block);
int ui_but_menu_direction(uiBut *but);
void ui_but_text_password_hide(char password_str[128], uiBut *but, bool restore);
/**
 * Finds the pressed button in an aligned row (typically an expanded enum).
 *
 * \param direction: Use when there may be multiple buttons pressed.
 */
uiBut *ui_but_find_select_in_enum(uiBut *but, int direction);
bool ui_but_is_editing(const uiBut *but);
float ui_block_calc_pie_segment(uiBlock *block, const float event_xy[2]);

/* XXX, this code will shorten any allocated string to 'UI_MAX_NAME_STR'
 * since this is really long its unlikely to be an issue,
 * but this could be supported */
void ui_but_add_shortcut(uiBut *but, const char *shortcut_str, bool do_strip);
void ui_but_clipboard_free();
bool ui_but_rna_equals(const uiBut *a, const uiBut *b);
bool ui_but_rna_equals_ex(const uiBut *but,
                          const PointerRNA *ptr,
                          const PropertyRNA *prop,
                          int index);
uiBut *ui_but_find_old(uiBlock *block_old, const uiBut *but_new);
uiBut *ui_but_find_new(uiBlock *block_new, const uiBut *but_old);

#ifdef WITH_INPUT_IME
void ui_but_ime_reposition(uiBut *but, int x, int y, bool complete);
const wmIMEData *ui_but_ime_data_get(uiBut *but);
#endif

/* interface_widgets.cc */

/* Widget shader parameters, must match the shader layout. */
struct uiWidgetBaseParameters {
  rctf recti, rect;
  float radi, rad;
  float facxi, facyi;
  float round_corners[4];
  float color_inner1[4], color_inner2[4];
  float color_outline[4], color_emboss[4];
  float color_tria[4];
  float tria1_center[2], tria2_center[2];
  float tria1_size, tria2_size;
  float shade_dir;
  /* We pack alpha check and discard factor in alpha_discard.
   * If the value is negative then we do alpha check.
   * The absolute value itself is the discard factor.
   * Initialize value to 1.0f if you don't want discard. */
  float alpha_discard;
  float tria_type;
  float _pad[3];
};

enum {
  ROUNDBOX_TRIA_NONE = 0,
  ROUNDBOX_TRIA_ARROWS,
  ROUNDBOX_TRIA_SCROLL,
  ROUNDBOX_TRIA_MENU,
  ROUNDBOX_TRIA_CHECK,
  ROUNDBOX_TRIA_HOLD_ACTION_ARROW,
  ROUNDBOX_TRIA_DASH,

  ROUNDBOX_TRIA_MAX, /* don't use */
};

blender::gpu::Batch *ui_batch_roundbox_widget_get();
blender::gpu::Batch *ui_batch_roundbox_shadow_get();

void ui_draw_menu_back(uiStyle *style, uiBlock *block, rcti *rect);
void ui_draw_popover_back(ARegion *region, uiStyle *style, uiBlock *block, rcti *rect);
void ui_draw_pie_center(uiBlock *block);
const uiWidgetColors *ui_tooltip_get_theme();

void ui_draw_widget_menu_back_color(const rcti *rect, bool use_shadow, const float color[4]);
void ui_draw_widget_menu_back(const rcti *rect, bool use_shadow);
void ui_draw_tooltip_background(const uiStyle *style, uiBlock *block, rcti *rect);

/**
 * Conversion from old to new buttons, so still messy.
 */
void ui_draw_but(const bContext *C, ARegion *region, uiStyle *style, uiBut *but, rcti *rect);

/**
 * Info about what the separator character separates, used to decide between different drawing
 * styles. E.g. we never want a shortcut string to be clipped, but other hint strings can be
 * clipped.
 */
enum uiMenuItemSeparatorType {
  UI_MENU_ITEM_SEPARATOR_NONE,
  /** Separator is used to indicate shortcut string of this item. Shortcut string will not get
   * clipped. */
  UI_MENU_ITEM_SEPARATOR_SHORTCUT,
  /** Separator is used to indicate some additional hint to display for this item. Hint string will
   * get clipped before the normal text. */
  UI_MENU_ITEM_SEPARATOR_HINT,
};
/**
 * Helper call to draw a menu item without a button.
 *
 * \param but_flag: Button flags (#uiBut.flag) indicating the state of the item, typically
 *                  #UI_HOVER, #UI_BUT_DISABLED, #UI_BUT_INACTIVE.
 * \param separator_type: The kind of separator which controls if and how the string is clipped.
 * \param r_xmax: The right hand position of the text, this takes into the icon, padding and text
 *                clipping when there is not enough room to display the full text.
 */
void ui_draw_menu_item(const uiFontStyle *fstyle,
                       rcti *rect,
                       const char *name,
                       int iconid,
                       int but_flag,
                       uiMenuItemSeparatorType separator_type,
                       int *r_xmax);
void ui_draw_preview_item(const uiFontStyle *fstyle,
                          rcti *rect,
                          const char *name,
                          int iconid,
                          int but_flag,
                          eFontStyle_Align text_align);
/**
 * Version of #ui_draw_preview_item() that does not draw the menu background and item text based on
 * state. It just draws the preview and text directly.
 *
 * \param draw_as_icon: Instead of stretching the preview/icon to the available width/height, draw
 *                      it at the standard icon size. Mono-icons will draw with \a text_col or the
 *                      corresponding theme override for this type of icon.
 */
void ui_draw_preview_item_stateless(const uiFontStyle *fstyle,
                                    rcti *rect,
                                    blender::StringRef name,
                                    int iconid,
                                    const uchar text_col[4],
                                    eFontStyle_Align text_align,
                                    bool draw_as_icon = false);

#define UI_TEXT_MARGIN_X 0.4f
#define UI_POPUP_MARGIN (UI_SCALE_FAC * 12)
/**
 * Margin at top of screen for popups.
 * Note this value must be sufficient to draw a popover arrow to avoid cropping it.
 */
#define UI_POPUP_MENU_TOP (int)(10 * UI_SCALE_FAC)

#define UI_PIXEL_AA_JITTER 8
extern const float ui_pixel_jitter[UI_PIXEL_AA_JITTER][2];

/* `interface_style.cc` */

/**
 * Called on each startup.blend read,
 * reading without #uiFont will create one.
 */
void uiStyleInit();

/* interface_icons.cc */

void ui_icon_ensure_deferred(const bContext *C, int icon_id, bool big);
int ui_id_icon_get(const bContext *C, ID *id, bool big);

/* interface_icons_event.cc */

void icon_draw_rect_input(
    float x, float y, int w, int h, float alpha, short event_type, short event_value);

/* resources.cc */

void ui_resources_init();
void ui_resources_free();

/* interface_layout.cc */

void ui_layout_add_but(uiLayout *layout, uiBut *but);
void ui_layout_remove_but(uiLayout *layout, const uiBut *but);
/**
 * \return true if the button was successfully replaced.
 */
bool ui_layout_replace_but_ptr(uiLayout *layout, const void *old_but_ptr, uiBut *new_but);
/**
 * \note May reallocate \a but, so the possibly new address is returned. May also override the
 *       #UI_BUT_DISABLED flag depending on if a search pointer-property pair was provided/found.
 */
uiBut *ui_but_add_search(uiBut *but,
                         PointerRNA *ptr,
                         PropertyRNA *prop,
                         PointerRNA *searchptr,
                         PropertyRNA *searchprop,
                         bool results_are_suggestions);
/**
 * Check all buttons defined in this layout,
 * and set any button flagged as UI_BUT_LIST_ITEM as active/selected.
 * Needed to handle correctly text colors of active (selected) list item.
 */
void ui_layout_list_set_labels_active(uiLayout *layout);
/* menu callback */
void ui_item_menutype_func(bContext *C, uiLayout *layout, void *arg_mt);
void ui_item_paneltype_func(bContext *C, uiLayout *layout, void *arg_pt);

/* interface_button_group.cc */

/**
 * Every function that adds a set of buttons must create another group,
 * then #ui_def_but adds buttons to the current group (the last).
 */
void ui_block_new_button_group(uiBlock *block, uiButtonGroupFlag flag);
void ui_button_group_add_but(uiBlock *block, uiBut *but);
void ui_button_group_replace_but_ptr(uiBlock *block, const uiBut *old_but_ptr, uiBut *new_but);

/* interface_drag.cc */

void ui_but_drag_free(uiBut *but);
bool ui_but_drag_is_draggable(const uiBut *but);
void ui_but_drag_start(bContext *C, uiBut *but);

/* interface_align.cc */

bool ui_but_can_align(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
int ui_but_align_opposite_to_area_align_get(const ARegion *region) ATTR_WARN_UNUSED_RESULT;
/**
 * Compute the alignment of all 'align groups' of buttons in given block.
 *
 * This is using an order-independent algorithm,
 * i.e. alignment of buttons should be OK regardless of order in which
 * they are added to the block.
 */
void ui_block_align_calc(uiBlock *block, const ARegion *region);

/* `interface_anim.cc` */

void ui_but_anim_flag(uiBut *but, const AnimationEvalContext *anim_eval_context);
void ui_but_anim_copy_driver(bContext *C);
void ui_but_anim_paste_driver(bContext *C);
/**
 * \a str can be NULL to only perform check if \a but has an expression at all.
 * \return if button has an expression.
 */
bool ui_but_anim_expression_get(uiBut *but, char *str, size_t str_maxncpy);
bool ui_but_anim_expression_set(uiBut *but, const char *str);
/**
 * Create new expression for button (i.e. a "scripted driver"), if it can be created.
 */
bool ui_but_anim_expression_create(uiBut *but, const char *str);
void ui_but_anim_autokey(bContext *C, uiBut *but, Scene *scene, float cfra);

void ui_but_anim_decorate_cb(bContext *C, void *arg_but, void *arg_dummy);
void ui_but_anim_decorate_update_from_flag(uiButDecorator *but);

/* `interface_query.cc` */

bool ui_but_is_editable(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_editable_as_text(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_toggle(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
/**
 * Can we mouse over the button or is it hidden/disabled/layout.
 * \note ctrl is kind of a hack currently,
 * so that non-embossed UI_BTYPE_TEXT button behaves as a label when ctrl is not pressed.
 */
bool ui_but_is_interactive_ex(const uiBut *but, const bool labeledit, const bool for_tooltip);
bool ui_but_is_interactive(const uiBut *but, bool labeledit) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_popover_once_compat(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_has_array_value(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
int ui_but_icon(const uiBut *but);
void ui_but_pie_dir(RadialDirection dir, float vec[2]);

bool ui_but_is_cursor_warp(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

bool ui_but_contains_pt(const uiBut *but, float mx, float my) ATTR_WARN_UNUSED_RESULT;
bool ui_but_contains_rect(const uiBut *but, const rctf *rect);
bool ui_but_contains_point_px_icon(const uiBut *but,
                                   ARegion *region,
                                   const wmEvent *event) ATTR_WARN_UNUSED_RESULT;
bool ui_but_contains_point_px(const uiBut *but, const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2, 3) ATTR_WARN_UNUSED_RESULT;

uiBut *ui_list_find_mouse_over(const ARegion *region,
                               const wmEvent *event) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_list_find_from_row(const ARegion *region, const uiBut *row_but) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_list_row_find_mouse_over(const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_list_row_find_index(const ARegion *region,
                              int index,
                              uiBut *listbox) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_view_item_find_mouse_over(const ARegion *region, const int xy[2]) ATTR_NONNULL(1, 2);
uiBut *ui_view_item_find_active(const ARegion *region);

using uiButFindPollFn = bool (*)(const uiBut *but, const void *customdata);
/**
 * x and y are only used in case event is NULL.
 */
uiBut *ui_but_find_mouse_over_ex(const ARegion *region,
                                 const int xy[2],
                                 bool labeledit,
                                 bool for_tooltip,
                                 const uiButFindPollFn find_poll,
                                 const void *find_custom_data)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_find_mouse_over(const ARegion *region, const wmEvent *event) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_find_rect_over(const ARegion *region, const rcti *rect_px) ATTR_WARN_UNUSED_RESULT;

uiBut *ui_list_find_mouse_over_ex(const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

bool ui_but_contains_password(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

blender::StringRef ui_but_drawstr_without_sep_char(const uiBut *but) ATTR_NONNULL();
size_t ui_but_drawstr_len_without_sep_char(const uiBut *but);
size_t ui_but_tip_len_only_first_line(const uiBut *but);

uiBut *ui_but_prev(uiBut *but) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_next(uiBut *but) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_first(uiBlock *block) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_last(uiBlock *block) ATTR_WARN_UNUSED_RESULT;

uiBut *ui_block_active_but_get(const uiBlock *block);
bool ui_block_is_menu(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
bool ui_block_is_popover(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
bool ui_block_is_pie_menu(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
bool ui_block_is_popup_any(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;

uiBlock *ui_block_find_mouse_over_ex(const ARegion *region, const int xy[2], bool only_clip)
    ATTR_NONNULL(1, 2);
uiBlock *ui_block_find_mouse_over(const ARegion *region, const wmEvent *event, bool only_clip);

uiBut *ui_region_find_first_but_test_flag(ARegion *region, int flag_include, int flag_exclude);
uiBut *ui_region_find_active_but(ARegion *region) ATTR_WARN_UNUSED_RESULT;
bool ui_region_contains_point_px(const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
bool ui_region_contains_rect_px(const ARegion *region, const rcti *rect_px);

/**
 * Check if the cursor is over any popups.
 */
ARegion *ui_screen_region_find_mouse_over_ex(bScreen *screen, const int xy[2]) ATTR_NONNULL(1, 2);
ARegion *ui_screen_region_find_mouse_over(bScreen *screen, const wmEvent *event);

/* interface_context_menu.cc */

bool ui_popup_context_menu_for_button(bContext *C, uiBut *but, const wmEvent *event);
/**
 * menu to show when right clicking on the panel header
 */
void ui_popup_context_menu_for_panel(bContext *C, ARegion *region, Panel *panel);

/* `interface_eyedropper.cc` */

wmKeyMap *eyedropper_modal_keymap(wmKeyConfig *keyconf);
wmKeyMap *eyedropper_colorband_modal_keymap(wmKeyConfig *keyconf);

/* interface_eyedropper_color.c */

void UI_OT_eyedropper_color(wmOperatorType *ot);

/* interface_eyedropper_colorband.c */

namespace blender::ui {
void UI_OT_eyedropper_colorramp(wmOperatorType *ot);
void UI_OT_eyedropper_colorramp_point(wmOperatorType *ot);
}  // namespace blender::ui

/* interface_eyedropper_datablock.c */

void UI_OT_eyedropper_id(wmOperatorType *ot);

/* interface_eyedropper_depth.c */

void UI_OT_eyedropper_depth(wmOperatorType *ot);

/* interface_eyedropper_driver.c */

void UI_OT_eyedropper_driver(wmOperatorType *ot);

/* interface_eyedropper_gpencil_color.c */

void UI_OT_eyedropper_gpencil_color(wmOperatorType *ot);

/* interface_template_asset_view.cc */

uiListType *UI_UL_asset_view();

/**
 * For use with #ui_rna_collection_search_update_fn.
 */
struct uiRNACollectionSearch {
  PointerRNA target_ptr;
  PropertyRNA *target_prop;

  PointerRNA search_ptr;
  PropertyRNA *search_prop;

  uiBut *search_but;
  /* Let UI_butstore_ API update search_but pointer above over redraws. */
  uiButStore *butstore;
  /* Block has to be stored for freeing butstore (uiBut.block doesn't work with undo). */
  uiBlock *butstore_block;
};
void ui_rna_collection_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, bool is_first);

/* `interface_ops.cc` */

bool ui_jump_to_target_button_poll(bContext *C);

/* interface_queries.c */

void ui_interface_tag_script_reload_queries();

/* interface_view.cc */

void ui_block_free_views(uiBlock *block);
void ui_block_views_bounds_calc(const uiBlock *block);
void ui_block_views_listen(const uiBlock *block, const wmRegionListenerParams *listener_params);
void ui_block_views_draw_overlays(const ARegion *region, const uiBlock *block);
blender::ui::AbstractView *ui_block_view_find_matching_in_old_block(
    const uiBlock &new_block, const blender::ui::AbstractView &new_view);

uiButViewItem *ui_block_view_find_matching_view_item_but_in_old_block(
    const uiBlock &new_block, const blender::ui::AbstractViewItem &new_item);

/* abstract_view_item.cc */

void ui_view_item_swap_button_pointers(blender::ui::AbstractViewItem &a,
                                       blender::ui::AbstractViewItem &b);

/* interface_templates.cc */

uiListType *UI_UL_cache_file_layers();

ID *ui_template_id_liboverride_hierarchy_make(
    bContext *C, Main *bmain, ID *owner_id, ID *id, const char **r_undo_push_label);

/* Functions in this namespace are only exposed for unit testing purposes, and
 * should not be used outside of the files where they are defined. */
namespace blender::interface::internal {

blender::Vector<FCurve *> get_property_drivers(
    PointerRNA *ptr, PropertyRNA *prop, bool get_all, int index, bool *r_is_array_prop);
int paste_property_drivers(blender::Span<FCurve *> src_drivers,
                           bool is_array_prop,
                           PointerRNA *dst_ptr,
                           PropertyRNA *dst_prop);

}  // namespace blender::interface::internal
