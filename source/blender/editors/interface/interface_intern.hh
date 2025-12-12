/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#pragma once

#include <functional>

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_fcurve.hh"

#include "DNA_listBase.h"

#include "RNA_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

struct AnimationEvalContext;
struct ARegion;
struct bContext;
struct bContextStore;
struct CurveMapping;
struct CurveProfile;
namespace blender::gpu {
class Batch;
}
struct IconTextOverlay;
struct ID;
struct ImBuf;
struct LayoutPanelHeader;
struct Main;
struct Scene;
namespace blender::ui {
struct HandleButtonData;
struct Layout;
struct UndoStack_Text;
}  // namespace blender::ui
struct uiListType;
struct uiStyle;
struct uiWidgetColors;
struct UnitSettings;
struct wmEvent;
struct wmKeyConfig;
struct wmOperatorType;
struct wmTimer;

namespace blender::ui {

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

/** #Button.flag */
enum {
  /** Use when the button is pressed. */
  UI_SELECT = (1 << 0),
  /** Temporarily hidden (scrolled out of the view). */
  UI_SCROLLED = (1 << 1),
  /**
   * The button is hovered by the mouse and should be drawn with a hover highlight. Also set
   * sometimes to highlight buttons without actually hovering it (e.g. for arrow navigation in
   * menus). UI handling code manages this mostly and usually does this together with making the
   * button active/focused (see #Button::active). This means events will be forwarded to it and
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
  BUT_ACTIVE_OVERRIDE = (1 << 7),

  /* WARNING: rest of #Button.flag in `UI_interface_c.hh`. */
};

/** #Button.pie_dir */
enum RadialDirection : int8_t {
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

/** #PieMenuData.flags */
enum {
  /** Use initial center of pie menu to calculate direction. */
  PIE_INITIAL_DIRECTION = (1 << 1),
  /** Pie menu is drag style. */
  PIE_DRAG_STYLE = (1 << 2),
  /** Mouse not far enough from center position. */
  PIE_INVALID_DIR = (1 << 3),
  /** Pie menu changed to click style, click to confirm. */
  PIE_CLICK_STYLE = (1 << 4),
  /** Pie animation finished, do not calculate any more motion. */
  PIE_ANIMATION_FINISHED = (1 << 5),
  /** Pie gesture selection has been done, now wait for mouse motion to end. */
  PIE_GESTURE_END_WAIT = (1 << 6),
};

#define PIE_CLICK_THRESHOLD_SQ 50.0f

/** The maximum number of items a radial menu (pie menu) can contain. */
#define PIE_MAX_ITEMS 8

struct Button {

  /** Pointer back to the layout item holding this button. */
  Layout *layout = nullptr;
  int flag = 0;
  int drawflag = 0;
  char flag2 = 0;

  ButtonType type = ButtonType(0);
  ButPointerType pointype = ButPointerType::None;
  bool bit = 0;
  /* 0-31 bit index. */
  char bitnr = 0;

  /** When non-zero, this is the key used to activate a menu items (`a-z` always lower case). */
  uchar menu_key = 0;

  short retval = 0, strwidth = 0, alignnr = 0;
  short ofs = 0, pos = 0, selsta = 0, selend = 0;

  /**
   * Optional color for monochrome icon. Also used as text
   * color for labels without icons. Set with #button_color_set().
   */
  uchar col[4] = {0};

  std::string str;

  std::string drawstr;

  char *placeholder = nullptr;

  /** Block relative coordinates. */
  rctf rect = {};

  char *poin = nullptr;
  float hardmin = 0, hardmax = 0, softmin = 0, softmax = 0;

  /** See \ref button_func_identity_compare_set(). */
  ButtonIdentityCompareFunc identity_cmp_func = nullptr;

  ButtonHandleFunc func = nullptr;
  void *func_arg1 = nullptr;
  void *func_arg2 = nullptr;
  /**
   * C++ version of #func above. Allows storing arbitrary data in a type safe way, no void
   * pointer arguments.
   */
  std::function<void(bContext &)> apply_func;

  ButtonHandleNFunc funcN = nullptr;
  void *func_argN = nullptr;
  ButtonArgNFree func_argN_free_fn;
  ButtonArgNCopy func_argN_copy_fn;

  const bContextStore *context = nullptr;

  ButtonCompleteFunc autocomplete_func = nullptr;
  void *autofunc_arg = nullptr;

  ButtonHandleRenameFunc rename_func = nullptr;
  void *rename_arg1 = nullptr;
  void *rename_orig = nullptr;

  /**
   * When defined, and the button edits a string RNA property,
   * the new name is _not_ set at all, instead this function is called with the new name.
   */
  std::function<void(std::string &new_name)> rename_full_func = nullptr;
  std::string rename_full_new;

  /** Run an action when holding the button down. */
  ButtonHandleHoldFunc hold_func = nullptr;
  void *hold_argN = nullptr;

  StringRef tip;
  ButtonToolTipFunc tip_func = nullptr;
  void *tip_arg = nullptr;
  FreeArgFunc tip_arg_free = nullptr;
  /** Function to override the label to be displayed in the tooltip. */
  std::function<std::string(const Button *)> tip_quick_func;

  ButtonToolTipCustomFunc tip_custom_func = nullptr;

  /** info on why button is disabled, displayed in tooltip */
  const char *disabled_info = nullptr;

  /** Little indicator (e.g., counter) displayed on top of some icons. */
  IconTextOverlay icon_overlay_text = {};

  /** Copied from the #Block.emboss */
  EmbossType emboss = EmbossType::Emboss;
  /** direction in a pie menu, used for collision detection. */
  RadialDirection pie_dir = UI_RADIAL_NONE;
  /** could be made into a single flag */
  bool changed = false;

  BIFIconID icon = ICON_NONE;

  /** Affects the order if this Button is used in menu-search. */
  float search_weight = 0.0f;

  short iconadd = 0;
  /** so buttons can support unit systems which are not RNA */
  uchar unit_type = 0;

  /** See #button_menu_disable_hover_open(). */
  bool menu_no_hover_open = false;

  /** #ButtonType::Block data */
  BlockCreateFunc block_create_func = nullptr;

  /** #ButtonType::Pulldown / #ButtonType::Menu data */
  MenuCreateFunc menu_create_func = nullptr;

  MenuStepFunc menu_step_func = nullptr;

  /* RNA data */
  PointerRNA rnapoin = {};
  PropertyRNA *rnaprop = nullptr;
  int rnaindex = 0;

  BIFIconID drag_preview_icon_id;
  void *dragpoin = nullptr;
  const ImBuf *imb = nullptr;
  float imb_scale = 0;
  eWM_DragDataType dragtype = WM_DRAG_ID;
  int8_t dragflag = 0;

  /**
   * Keep an operator attached but never actually call it through the button. See
   * #button_operator_set_never_call().
   */
  bool operator_never_call = false;
  /* Operator data */
  wm::OpCallContext opcontext = wm::OpCallContext::InvokeDefault;
  wmOperatorType *optype = nullptr;
  PointerRNA *opptr = nullptr;

  ListBase extra_op_icons = {nullptr, nullptr}; /** #ButtonExtraOpIcon */

  /**
   * Active button data, set when the user is hovering or interacting with a button (#UI_HOVER and
   * #UI_SELECT state mostly).
   */
  HandleButtonData *active = nullptr;
  /**
   * Event handling only supports one active button at a time, but there are cases where that's not
   * enough. A common one is to keep some filter button active to receive text input, while other
   * buttons remain active for interaction.
   *
   * Buttons that have #semi_modal_state set will be temporarily activated for event handling. If
   * they don't consume the event (for example text input events) the event will be forwarded to
   * other buttons.
   *
   * Currently only text buttons support this well.
   */
  HandleButtonData *semi_modal_state = nullptr;

  /** Custom button data (borrowed, not owned). */
  void *custom_data = nullptr;

  char *editstr = nullptr;
  double *editval = nullptr;
  float *editvec = nullptr;

  std::function<bool(const Button &)> pushed_state_func;

  /* pointer back */
  Block *block = nullptr;

  Button() = default;
  /** Performs a mostly shallow copy for now. Only contained C++ types are deep copied. */
  Button(const Button &other) = default;
  /** Mostly shallow copy, just like copy constructor above. */
  Button &operator=(const Button &other) = default;

  virtual ~Button() = default;
};

/** Derived struct for #ButtonType::Num */
struct ButtonNumber : public Button {
  float step_size = 0.0f;
  float precision = 0.0f;
};

/** Derived struct for #ButtonType::NumSlider */
struct ButtonNumberSlider : public Button {
  float step_size = 0.0f;
  float precision = 0.0f;
};

/** Derived struct for #ButtonType::Color */
struct ButtonColor : public Button {
  bool is_pallete_color = false;
  int palette_color_index = -1;
};

/** Derived struct for #ButtonType::Tab */
struct ButtonTab : public Button {
  MenuType *menu = nullptr;
};

/** Derived struct for #ButtonType::SearchMenu */
struct ButtonSearch : public Button {
  ButtonSearchCreateFn popup_create_fn = nullptr;
  ButtonSearchUpdateFn items_update_fn = nullptr;
  ButtonSearchListenFn listen_fn = nullptr;

  void *item_active = nullptr;
  char *item_active_str;

  void *arg = nullptr;
  FreeArgFunc arg_free_fn = nullptr;

  ButtonSearchContextMenuFn item_context_menu_fn = nullptr;
  ButtonSearchTooltipFn item_tooltip_fn = nullptr;

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
 * Derived struct for #ButtonType::Decorator
 * Decorators have their own RNA data, using the normal #Button RNA members has many side-effects.
 */
struct ButtonDecorator : public Button {
  PointerRNA decorated_rnapoin = {};
  PropertyRNA *decorated_rnaprop = nullptr;
  int decorated_rnaindex = -1;
  /* The only action allowed to decorators currently is to set or clear animation keyframes.
   * However, they should be able to do it only under some circumstances (typically, when they do
   * display animation-related status). */
  bool toggle_keyframe_on_click = false;
};

/** Derived struct for #ButtonType::Progress. */
struct ButtonProgress : public Button {
  /** Progress in 0..1 range. */
  float progress_factor = 0.0f;
  /** The display style (bar, pie... etc). */
  ButProgressType progress_type = ButProgressType::Bar;
};

/** Derived struct for #ButtonType::SeprLine. */
struct ButtonSeparatorLine : public Button {
  bool is_vertical;
};

/** Derived struct for #ButtonType::Label. */
struct ButtonLabel : public Button {
  float alpha_factor = 1.0f;
};

/** Derived struct for #ButtonType::Scroll. */
struct ButtonScrollBar : public Button {
  /** Actual visual height of UI list (in rows). */
  float visual_height = -1.0f;
};

struct ButtonViewItem : public Button {
  /** The view item this button was created for. */
  AbstractViewItem *view_item = nullptr;
  /**
   * Some items want to have a fixed size for drawing, differing from the interaction rectangle
   * (e.g. so highlights are drawn smaller).
   */
  int draw_width = 0;
  int draw_height = 0;
};

/** Derived struct for #ButtonType::HsvCube. */
struct ButtonHSVCube : public Button {
  eButGradientType gradient_type = GRAD_SV;
};

/** Derived struct for #ButtonType::ColorBand. */
struct ButtonColorBand : public Button {
  ColorBand *edit_coba = nullptr;
};

/** Derived struct for #ButtonType::CurveProfile. */
struct ButtonCurveProfile : public Button {
  CurveProfile *edit_profile = nullptr;
};

/** Derived struct for #ButtonType::Curve. */
struct ButtonCurveMapping : public Button {
  CurveMapping *edit_cumap = nullptr;
  eButGradientType gradient_type = GRAD_SV;
};

/** Derived struct for #ButtonType::HotkeyEvent. */
struct ButtonHotkeyEvent : public Button {
  wmEventModifierFlag modifier_key = wmEventModifierFlag(0);
};

/**
 * Additional, superimposed icon for a button, invoking an operator.
 */
struct ButtonExtraOpIcon {
  ButtonExtraOpIcon *next, *prev;

  BIFIconID icon;
  wmOperatorCallParams *optype_params;

  bool highlighted;
  bool disabled;
};

struct ColorPicker {
  ColorPicker *next, *prev;

  /**
   * Color in HSV or HSL, in color picking color space. Used for HSV cube,
   * circle and slider widgets. The color picking space is perceptually
   * linear for intuitive editing.
   */
  float hsv_perceptual[3];
  /** Initial color data (to detect changes). */
  float hsv_perceptual_init[3];
  bool is_init;

  /**
   * HSV or HSL in color picker space used for number sliders.
   */
  float hsv_perceptual_slider[3];
  float hsv_linear_slider[3];

  /*
   * RGB in color picker used for number sliders, when the space is not scene linear.
   * When it is linear, the RNA property is used directly so that keyframing works.
   */
  float rgb_perceptual_slider[3];

  /* Hex Color string */
  char hexcol[128];

  /** Cubic saturation for the color wheel. */
  bool use_color_cubic;
  bool use_color_lock;
  bool use_luminosity_lock;
  float luminosity_lock_value;

  /** Alpha component. */
  bool has_alpha;
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

/** #Block.content_hints */
enum eBlockContentHints {
  /** In a menu block, if there is a single sub-menu button, we add some
   * padding to the right to put nicely aligned triangle icons there. */
  BLOCK_CONTAINS_SUBMENU_BUT = (1 << 0),
};

/* #ButtonGroup.flag. */
enum ButtonGroupFlag {
  /** While this flag is set, don't create new button groups for layout item calls. */
  UI_BUTTON_GROUP_LOCK = (1 << 0),
  /** The buttons in this group are inside a panel header. */
  UI_BUTTON_GROUP_PANEL_HEADER = (1 << 1),
};
ENUM_OPERATORS(ButtonGroupFlag);

/**
 * A group of button references, used by property search to keep track of sets of buttons that
 * should be searched together. For example, in property split layouts number buttons and their
 * labels (and even their decorators) are separate buttons, but they must be searched and
 * highlighted together.
 */
struct ButtonGroup {
  Vector<Button *> buttons;
  ButtonGroupFlag flag;
};

struct BlockDynamicListener {
  BlockDynamicListener *next, *prev;

  void (*listener_func)(const wmRegionListenerParams *params);
};

enum class BlockAlertLevel : int8_t { None, Info, Success, Warning, Error };

struct Block {
  Block *next, *prev;

  Vector<std::unique_ptr<Button>> buttons;
  Panel *panel;
  Block *oldblock;

  /** Used for `UI_butstore_*` runtime function. */
  ListBase butstore;

  Vector<ButtonGroup> button_groups;

  ListBase layouts;
  Layout *curlayout;

  Vector<std::unique_ptr<bContextStore>> contexts;

  /** A block can store "views" on data-sets. Currently tree-views (#AbstractTreeView) only.
   * Others are imaginable, e.g. table-views, grid-views, etc. These are stored here to support
   * state that is persistent over redraws (e.g. collapsed tree-view items). */
  ListBase views;

  ListBase dynamic_listeners; /* #BlockDynamicListener */

  std::string name;

  float winmat[4][4];

  rctf rect;
  float aspect;

  BlockAlertLevel alert_level = BlockAlertLevel::None;

  /** Unique hash used to implement popup menu memory. */
  uint puphash;

  ButtonHandleFunc func;
  void *func_arg1;
  void *func_arg2;

  ButtonHandleNFunc funcN;
  void *func_argN;
  ButtonArgNFree func_argN_free_fn;
  ButtonArgNCopy func_argN_copy_fn;

  BlockHandleFunc handle_func;
  void *handle_func_arg;

  /** Custom interaction data. */
  BlockInteraction_CallbackData custom_interaction_callbacks;

  /** Custom extra event handling. */
  int (*block_event_func)(const bContext *C, Block *, const wmEvent *);

  /** Custom extra draw function for custom blocks. */
  std::function<void(const bContext *, rcti *)> drawextra;

  int flag;
  short alignnr;
  /** Hints about the buttons of this block. Used to avoid iterating over
   * buttons to find out if some criteria is met by any. Instead, check this
   * criteria when adding the button and set a flag here if it's met. */
  short content_hints; /* #eBlockContentHints */

  char direction;
  /** BLOCK_THEME_STYLE_* */
  char theme_style;
  /** Copied to #Button.emboss */
  EmbossType emboss;
  bool auto_open;
  char _pad[5];
  double auto_open_last;

  const char *lockstr;

  bool lock;
  /** To keep blocks while drawing and free them afterwards. */
  bool active;
  /** To avoid tool-tip after click. */
  bool tooltipdisabled;
  /** True when #block_end has been called. */
  bool endblock;

  /** for doing delayed */
  BlockBoundsCalc bounds_type;
  /** Offset to use when calculating bounds (in pixels). */
  int bounds_offset[2];
  /** for doing delayed */
  int bounds, minbounds;

  /** Pull-downs, to detect outside, can differ per case how it is created. */
  rctf safety;
  /** #SafetyRect list */
  ListBase saferct;

  PopupBlockHandle *handle;

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

  void remove_but(const Button *but);
  [[nodiscard]] Button *first_but() const;
  [[nodiscard]] Button *last_but() const;
  int but_index(const Button *but) const;
  [[nodiscard]] Button *next_but(const Button *but) const;
  [[nodiscard]] Button *prev_but(const Button *but) const;
};

struct SafetyRect {
  SafetyRect *next, *prev;
  rctf parent;
  rctf safety;
};
/* `interface.cc` */

void fontscale(float *points, float aspect);

/** Project button or block (but==nullptr) to pixels in region-space. */
void button_to_pixelrect(rcti *rect, const ARegion *region, const Block *block, const Button *but);
rcti ui_to_pixelrect(const ARegion *region, const Block *block, const rctf *src_rect);

void block_to_region_fl(const ARegion *region, const Block *block, float *x, float *y);
void block_to_window_fl(const ARegion *region, const Block *block, float *x, float *y);
void block_to_window(const ARegion *region, const Block *block, int *x, int *y);
void block_to_region_rctf(const ARegion *region,
                          const Block *block,
                          rctf *rct_dst,
                          const rctf *rct_src);
void block_to_window_rctf(const ARegion *region,
                          const Block *block,
                          rctf *rct_dst,
                          const rctf *rct_src);
float block_to_window_scale(const ARegion *region, const Block *block);
/**
 * For mouse cursor.
 */
void window_to_block_fl(const ARegion *region, const Block *block, float *x, float *y);
void window_to_block(const ARegion *region, const Block *block, int *x, int *y);
void window_to_block_rctf(const ARegion *region,
                          const Block *block,
                          rctf *rct_dst,
                          const rctf *rct_src);
void window_to_region(const ARegion *region, int *x, int *y);
void window_to_region_rcti(const ARegion *region, rcti *rect_dst, const rcti *rct_src);
void window_to_region_rctf(const ARegion *region, rctf *rect_dst, const rctf *rct_src);
void region_to_window(const ARegion *region, int *x, int *y);
void region_to_window(
    const ARegion *region, int region_x, int region_y, int *r_window_x, int *r_window_y);
/**
 * Popups will add a margin to #ARegion.winrct for shadow,
 * for interactivity (point-inside tests for eg), we want the winrct without the margin added.
 */
void region_winrct_get_no_margin(const ARegion *region, rcti *r_rect);

/** Register a listener callback to this block to tag the area/region for redraw. */
void block_add_dynamic_listener(Block *block,
                                void (*listener_func)(const wmRegionListenerParams *params));

double button_value_get(Button *but);
void button_value_set(Button *but, double value);
/**
 * For picker, while editing HSV.
 */
void button_hsv_set(Button *but);
/**
 * For buttons pointing to color for example.
 */
void button_v3_get(Button *but, float vec[3]);
void button_v3_set(Button *but, const float vec[3]);
void button_v4_get(Button *but, float vec[4]);
void button_v4_set(Button *but, const float vec[4]);

void hsvcircle_vals_from_pos(
    const rcti *rect, float mx, float my, float *r_val_rad, float *r_val_dist);
/**
 * Cursor in HSV circle, in float units -1 to 1, to map on radius.
 */
void hsvcircle_pos_from_vals(
    const ColorPicker *cpicker, const rcti *rect, const float *hsv, float *r_xpos, float *r_ypos);
void hsvcube_pos_from_vals(
    const ButtonHSVCube *hsv_but, const rcti *rect, const float *hsv, float *r_xp, float *r_yp);

/**
 * \param float_precision: For number buttons the precision
 * to use or -1 to fall back to the button default.
 * \param use_exp_float: Use exponent representation of floats
 * when out of reasonable range (outside of 1e3/1e-3).
 */
void button_string_get_ex(Button *but,
                          char *str,
                          size_t str_maxncpy,
                          int float_precision,
                          bool use_exp_float,
                          bool *r_use_exp_float) ATTR_NONNULL(1, 2);
void button_string_get(Button *but, char *str, size_t str_maxncpy) ATTR_NONNULL();
/**
 * A version of #button_string_get_ex for dynamic buffer sizes
 * (where #button_string_get_maxncpy returns 0).
 *
 * \param r_str_size: size of the returned string (including terminator).
 */
char *button_string_get_dynamic(Button *but, int *r_str_size);
/**
 * \param str: will be overwritten.
 */
void button_convert_to_unit_alt_name(Button *but, char *str, size_t str_maxncpy) ATTR_NONNULL();
bool button_string_set(bContext *C, Button *but, const char *str) ATTR_NONNULL();
bool button_string_eval_number(bContext *C, const Button *but, const char *str, double *r_value)
    ATTR_NONNULL();
int button_string_get_maxncpy(Button *but);
/**
 * Clear & exit the active button's string..
 */
void button_active_string_clear_and_exit(bContext *C, Button *but) ATTR_NONNULL();
/**
 * Use handling code to set a string for the button. Handles the case where the string is set for a
 * search button while the search menu is open, so the results are updated accordingly.
 * This is basically the same as pasting the string into the button.
 */
void button_set_string_interactive(bContext *C, Button *but, const char *value);
Button *button_drag_multi_edit_get(Button *but);

/**
 * Get the hint that describes the expected value when empty.
 */
const char *button_placeholder_get(Button *but);

void def_but_icon(Button *but, int icon, int flag);
/**
 * Avoid using this where possible since it's better not to ask for an icon in the first place.
 */
void def_but_icon_clear(Button *but);

void button_extra_operator_icons_free(Button *but);

void button_rna_menu_convert_to_panel_type(Button *but, const char *panel_type);
void button_rna_menu_convert_to_menu_type(Button *but, const char *menu_type);
bool button_menu_draw_as_popover(const Button *but);

void button_range_set_hard(Button *but);
void button_range_set_soft(Button *but);

bool button_context_poll_operator(bContext *C, wmOperatorType *ot, const Button *but);
/**
 * Check if the operator \a ot poll is successful with the context given by \a but (optionally).
 * \param but: The button that might store context. Can be NULL for convenience (e.g. if there is
 *             no button to take context from, but we still want to poll the operator).
 */
bool button_context_poll_operator_ex(bContext *C,
                                     const Button *but,
                                     const wmOperatorCallParams *optype_params);

void button_update(Button *but);
void button_update_edited(Button *but);
PropertyScaleType button_scale_type(const Button *but) ATTR_WARN_UNUSED_RESULT;
bool button_is_float(const Button *but) ATTR_WARN_UNUSED_RESULT;
bool button_is_bool(const Button *but) ATTR_WARN_UNUSED_RESULT;
bool button_is_unit(const Button *but) ATTR_WARN_UNUSED_RESULT;
/**
 * Check if this button is similar enough to be grouped with another.
 */
bool button_is_compatible(const Button *but_a, const Button *but_b) ATTR_WARN_UNUSED_RESULT;
bool button_is_rna_valid(Button *but) ATTR_WARN_UNUSED_RESULT;
/**
 * Checks if the button supports cycling next/previous menu items (ctrl+mouse-wheel).
 */
bool button_supports_cycling(const Button *but) ATTR_WARN_UNUSED_RESULT;

/**
 * Check if the button is pushed, this is only meaningful for some button types.
 *
 * \return (0 == UNSELECT), (1 == SELECT), (-1 == DO-NOTHING)
 */
int button_is_pushed_ex(Button *but, double *value) ATTR_WARN_UNUSED_RESULT;
int button_is_pushed(Button *but) ATTR_WARN_UNUSED_RESULT;

void button_override_flag(Main *bmain, Button *but);

void block_bounds_calc(Block *block);

const ColorManagedDisplay *block_cm_display_get(Block *block);
void block_cm_to_display_space_v3(Block *block, float pixel[3]);

/* `interface_regions.cc` */

struct KeyNavLock {
  /** Set when we're using keyboard-input. */
  bool is_keynav = false;
  /** Only used to check if we've moved the cursor. */
  int2 event_xy = int2(0);
};

using BlockHandleCreateFunc = Block *(*)(bContext * C, PopupBlockHandle *handle, void *arg1);

struct PopupBlockCreate {
  BlockCreateFunc create_func = nullptr;
  BlockHandleCreateFunc handle_create_func = nullptr;
  void *arg = nullptr;
  FreeArgFunc arg_free = nullptr;

  int2 event_xy = int2(0);

  /** Set when popup is initialized from a button. */
  ARegion *butregion = nullptr;
  Button *but = nullptr;
};

struct PopupBlockHandle {
  /* internal */
  ARegion *region = nullptr;

  /** Use only for #BLOCK_MOVEMOUSE_QUIT popups. */
  float towards_xy[2];
  double towardstime = 0.0;
  bool dotowards = false;

  bool popup = false;
  void (*popup_func)(bContext *C, void *arg, int event) = nullptr;
  void (*cancel_func)(bContext *C, void *arg) = nullptr;
  void *popup_arg = nullptr;

  /** Store data for refreshing popups. */
  PopupBlockCreate popup_create_vars;
  /**
   * True if we can re-create the popup using #PopupBlockHandle.popup_create_vars.
   *
   * \note Popups that can refresh are called with #bContext::wm::region_popup set
   * to the #PopupBlockHandle::region both on initial creation and when refreshing.
   */
  bool can_refresh = false;
  bool refresh = false;

  wmTimer *scrolltimer = nullptr;
  float scrolloffset = 0.0f;

  KeyNavLock keynav_state;

  /* for operator popups */
  wmOperator *popup_op = nullptr;
  ScrArea *ctx_area = nullptr;
  ARegion *ctx_region = nullptr;

  /* return values */
  int butretval = 0;
  int menuretval = 0;
  int retvalue = 0;
  float retvec[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  /** Menu direction. */
  int direction = 0;

  /* Previous values so we don't resize or reposition on refresh. */
  rctf prev_block_rect = {};
  rctf prev_butrct = {};
  short prev_dir1 = 0;
  short prev_dir2 = 0;
  int prev_bounds_offset[2] = {0, 0};

  /* Maximum estimated size to avoid having to reposition on refresh. */
  float max_size_x = 0.0f;
  float max_size_y = 0.0f;

  /* #ifdef USE_DRAG_POPUP */
  bool is_grab = false;
  int grab_xy_prev[2] = {0, 0};
  /* #endif */

  char menu_idname[64] = "";
};

/* -------------------------------------------------------------------- */
/** \name Interface Region Functions
 *
 * `interface_region_*.cc` sources.
 * \{ */

/* `interface_region_tooltip.cc` */

/* exposed as public API in UI_interface.hh */

/* `interface_region_color_picker.cc` */

void color_picker_rgb_to_hsv_compat(const float rgb[3], float r_cp[3]);
void color_picker_rgb_to_hsv(const float rgb[3], float r_cp[3]);
void color_picker_hsv_to_rgb(const float r_cp[3], float rgb[3]);

/**
 * Returns true if the button is for a color with gamma baked in,
 * or if it's a color picker for such a button.
 */
bool button_is_color_gamma(Button *but);

/**
 * Returns true if the button represents a color with an Alpha component.
 */
bool button_color_has_alpha(Button *but);

void scene_linear_to_perceptual_space(Button *but, float rgb[3]);
void perceptual_to_scene_linear_space(Button *but, float rgb[3]);

Block *block_func_COLOR(bContext *C, PopupBlockHandle *handle, void *arg_but);
ColorPicker *block_colorpicker_create(Block *block);

/* `interface_region_search.cc` */

/**
 * Search-box for string button.
 */
ARegion *searchbox_create_generic(bContext *C, ARegion *butregion, ButtonSearch *search_but);
ARegion *searchbox_create_operator(bContext *C, ARegion *butregion, ButtonSearch *search_but);
ARegion *searchbox_create_menu(bContext *C, ARegion *butregion, ButtonSearch *search_but);

/**
 * x and y in screen-coords.
 */
bool searchbox_inside(ARegion *region, const int xy[2]) ATTR_NONNULL(1, 2);
int searchbox_find_index(ARegion *region, const char *name);
/**
 * Region is the search box itself.
 */
void searchbox_update(bContext *C, ARegion *region, Button *but, bool reset);
int searchbox_autocomplete(bContext *C, ARegion *region, Button *but, char *str);
bool searchbox_event(
    bContext *C, ARegion *region, Button *but, ARegion *butregion, const wmEvent *event);
/**
 * String validated to be of correct length (but->hardmax).
 */
bool searchbox_apply(Button *but, ARegion *region);
void searchbox_free(bContext *C, ARegion *region);
/**
 * XXX weak: search_func adds all partial matches.
 */
void button_search_refresh(ButtonSearch *but);

/* `interface_region_menu_popup.cc` */

int button_menu_step(Button *but, int direction);
bool button_menu_step_poll(const Button *but);
Button *popup_menu_memory_get(Block *block);
void popup_menu_memory_set(Block *block, Button *but);

/**
 * Called for creating new popups and refreshing existing ones.
 */
Block *popup_block_refresh(bContext *C, PopupBlockHandle *handle, ARegion *butregion, Button *but);

PopupBlockHandle *popup_block_create(bContext *C,
                                     ARegion *butregion,
                                     Button *but,
                                     BlockCreateFunc create_func,
                                     BlockHandleCreateFunc handle_create_func,
                                     void *arg,
                                     FreeArgFunc arg_free,
                                     bool can_refresh);
PopupBlockHandle *popup_menu_create(
    bContext *C, ARegion *butregion, Button *but, MenuCreateFunc menu_func, void *arg);

/* `interface_region_popover.cc` */

using PopoverCreateFunc = std::function<void(bContext *, Layout *, PanelType *)>;

PopupBlockHandle *popover_panel_create(bContext *C,
                                       ARegion *butregion,
                                       Button *but,
                                       PopoverCreateFunc popover_func,
                                       const PanelType *panel_type);

/* `interface_region_menu_pie.cc` */

/**
 * Set up data for defining a new pie menu level and add button that invokes it.
 */
void pie_menu_level_create(Block *block,
                           wmOperatorType *ot,
                           StringRefNull propname,
                           IDProperty *properties,
                           const EnumPropertyItem *items,
                           int totitem,
                           wm::OpCallContext context,
                           eUI_Item_Flag flag);

/* `interface_region_popup.cc` */

/**
 * Translate any popup regions (so we can drag them).
 */
void popup_translate(ARegion *region, const int mdiff[2]);
void popup_block_free(bContext *C, PopupBlockHandle *handle);
void popup_block_scrolltest(Block *block);

/** \} */

/* `interface_panel.cc` */

/**
 * Handle region panel events like opening and closing panels, changing categories, etc.
 *
 * \note Could become a modal key-map.
 */
int handler_panel_region(bContext *C,
                         const wmEvent *event,
                         ARegion *region,
                         const Button *active_but);
/**
 * Draw a panel integrated in buttons-window, tool/property lists etc.
 */
void draw_aligned_panel(const ARegion *region,
                        const uiStyle *style,
                        const Block *block,
                        const rcti *rect,
                        bool show_pin,
                        bool show_background,
                        bool region_search_filter_active);
void draw_layout_panels_backdrop(const ARegion *region,
                                 const Panel *panel,
                                 const float radius,
                                 float subpanel_backcolor[4]);
void panel_drag_collapse_handler_add(const bContext *C, const bool was_open);
void panel_tag_search_filter_match(Panel *panel);
/** Toggles layout panel open state and returns the new state. */
bool ui_layout_panel_toggle_open(const bContext *C, LayoutPanelHeader *header);
LayoutPanelHeader *layout_panel_header_under_mouse(const Panel &panel, const int my);
/** Apply scroll to layout panels when the main panel is used in popups. */
void layout_panel_popup_scroll_apply(Panel *panel, const float dy);

/**
 * Draws in resolution of 48x4 colors.
 */
void draw_gradient(const rcti *rect,
                   const float hsv[3],
                   eButGradientType type,
                   float alpha,
                   const ColorManagedDisplay *display);

/**
 * Draws rounded corner segments but inverted. Imagine each corner like a filled right triangle,
 * just that the hypotenuse is nicely curved inwards (towards the right angle of the triangle).
 *
 * Useful for connecting orthogonal shapes with a rounded corner, which can look quite nice.
 */
void draw_rounded_corners_inverted(const rcti &rect, const float rad, const float4 color);

void draw_but_TAB_outline(const rcti *rect,
                          float rad,
                          uchar highlight[3],
                          uchar highlight_fade[3]);
void draw_but_HISTOGRAM(ARegion *region,
                        Button *but,
                        const uiWidgetColors *wcol,
                        const rcti *recti);
void draw_but_WAVEFORM(ARegion *region,
                       Button *but,
                       const uiWidgetColors *wcol,
                       const rcti *recti);
void draw_but_VECTORSCOPE(ARegion *region,
                          Button *but,
                          const uiWidgetColors *wcol,
                          const rcti *recti);
void draw_but_COLORBAND(Button *but, const uiWidgetColors *wcol, const rcti *rect);
void draw_but_UNITVEC(Button *but, const uiWidgetColors *wcol, const rcti *rect, float radius);
void draw_but_CURVE(ARegion *region, Button *but, const uiWidgetColors *wcol, const rcti *rect);
/**
 * Draws the curve profile widget. Somewhat similar to ui_draw_but_CURVE.
 */
void draw_but_CURVEPROFILE(ARegion *region,
                           Button *but,
                           const uiWidgetColors *wcol,
                           const rcti *rect);
void draw_but_IMAGE(ARegion *region, Button *but, const uiWidgetColors *wcol, const rcti *rect);
void draw_but_TRACKPREVIEW(ARegion *region,
                           Button *but,
                           const uiWidgetColors *wcol,
                           const rcti *recti);

/* `interface_undo.cc` */

/**
 * Start the undo stack.
 *
 * \note The current state should be pushed immediately after calling this.
 */
UndoStack_Text *textedit_undo_stack_create();
void textedit_undo_stack_destroy(UndoStack_Text *stack);
/**
 * Push the information in the arguments to a new state in the undo stack.
 *
 * \note Currently the total length of the undo stack is not limited.
 */
void textedit_undo_push(UndoStack_Text *stack, const char *text, int cursor_index);
const char *textedit_undo(UndoStack_Text *stack, int direction, int *r_cursor_index);

/* `interface_handlers.cc` */

void but_handle_data_free(HandleButtonData **data);

void handle_afterfunc_add_operator(wmOperatorType *ot, wm::OpCallContext opcontext);
/**
 * Assumes event type is MOUSEPAN.
 */
void pan_to_scroll(const wmEvent *event, int *type, int *val);
/**
 * Exported to `interface.cc`: #button_active_only()
 * \note The region is only for the button.
 * The context needs to be set by the caller.
 */
void button_activate_event(bContext *C, ARegion *region, Button *but);
/**
 * Simulate moving the mouse over a button (or navigating to it with arrow keys).
 *
 * exported so menus can start with a highlighted button,
 * even if the mouse isn't over it
 */
void button_activate_over(bContext *C, ARegion *region, Button *but);
void button_execute_begin(bContext *C, ARegion *region, Button *but, void **active_back);
void button_execute_end(bContext *C, ARegion *region, Button *but, void *active_back);
void button_active_free(const bContext *C, Button *but);
void button_semi_modal_state_free(const bContext *C, Button *but);
/**
 * In some cases we may want to update the view (#View2D) in-between layout definition and drawing.
 * E.g. to make sure a button is visible while editing.
 */
void button_update_view_for_active(const bContext *C, const Block *block);
int button_menu_direction(Button *but);
void button_text_password_hide(std::string &password_str, Button *but, bool restore);
/**
 * Finds the pressed button in an aligned row (typically an expanded enum).
 *
 * \param direction: Use when there may be multiple buttons pressed.
 */
Button *button_find_select_in_enum(Button *but, int direction);
bool button_is_editing(const Button *but);
float block_calc_pie_segment(Block *block, const float event_xy[2]);

/* XXX, this code will shorten any allocated string to 'UI_MAX_NAME_STR'
 * since this is really long its unlikely to be an issue,
 * but this could be supported */
void button_add_shortcut(Button *but, const char *shortcut_str, bool do_strip);
void button_clipboard_free();
bool button_rna_equals(const Button *a, const Button *b);
bool button_rna_equals_ex(const Button *but,
                          const PointerRNA *ptr,
                          const PropertyRNA *prop,
                          int index);
Button *button_find_old(Block *block_old, const Button *but_new);
Button *button_find_new(Block *block_new, const Button *but_old);

#ifdef WITH_INPUT_IME
void button_ime_reposition(Button *but, int x, int y, bool complete);
const wmIMEData *button_ime_data_get(Button *but);
#endif

/* `interface_widgets.cc` */

/** Widget shader parameters, must match the shader layout. */
struct WidgetBaseParameters {
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

gpu::Batch *batch_roundbox_widget_get();
gpu::Batch *batch_roundbox_shadow_get();

void draw_menu_back(uiStyle *style, Block *block, const rcti *rect);
void draw_popover_back(ARegion *region, uiStyle *style, Block *block, const rcti *rect);
void draw_pie_center(Block *block);
const uiWidgetColors *tooltip_get_theme();

void draw_widget_menu_back_color(const rcti *rect, bool use_shadow, const float color[4]);
void draw_widget_menu_back(const rcti *rect, bool use_shadow);
void draw_tooltip_background(const uiStyle *style, Block *block, const rcti *rect);

/**
 * Conversion from old to new buttons, so still messy.
 */
void draw_button(const bContext *C, ARegion *region, uiStyle *style, Button *but, rcti *rect);

/**
 * Info about what the separator character separates, used to decide between different drawing
 * styles. E.g. we never want a shortcut string to be clipped, but other hint strings can be
 * clipped.
 */
enum MenuItemSeparatorType {
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
 * \param back_rect: Used to draw/leave out the backdrop of the menu item. Useful when layering
 *                   multiple items with different formatting like in search menus.
 * \param but_flag: Button flags (#Button.flag) indicating the state of the item, typically
 *                  #UI_HOVER, #BUT_DISABLED, #BUT_INACTIVE.
 * \param separator_type: The kind of separator which controls if and how the string is clipped.
 * \param r_xmax: The right hand position of the text, this takes into the icon, padding and text
 *                clipping when there is not enough room to display the full text.
 */
void draw_menu_item(const uiFontStyle *fstyle,
                    rcti *rect,
                    rcti *back_rect,
                    float zoom,
                    bool use_unpadded,
                    const char *name,
                    int iconid,
                    int but_flag,
                    MenuItemSeparatorType separator_type,
                    int *r_xmax);
void draw_preview_item(const uiFontStyle *fstyle,
                       rcti *rect,
                       float zoom,
                       const char *name,
                       int iconid,
                       int but_flag,
                       FontStyleAlign text_align);
/**
 * Version of #ui_draw_preview_item() that does not draw the menu background and item text based on
 * state. It just draws the preview and text directly.
 *
 * \param draw_as_icon: Instead of stretching the preview/icon to the available width/height, draw
 *                      it at the standard icon size. Mono-icons will draw with \a text_col or the
 *                      corresponding theme override for this type of icon.
 */
void draw_preview_item_stateless(const uiFontStyle *fstyle,
                                 rcti *rect,
                                 StringRef name,
                                 int iconid,
                                 const uchar text_col[4],
                                 FontStyleAlign text_align,
                                 const bool add_padding);

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
void style_init();

/* `interface_icons.cc` */

void icon_ensure_deferred(const bContext *C, int icon_id, bool big);
/** Is \a icon_id a preview icon that is being loaded/rendered? */
bool icon_is_preview_deferred_loading(int icon_id, bool big);
int id_icon_get(const bContext *C, ID *id, bool big);

/* `interface_icons_event.cc` */

float event_icon_offset(int icon_id);

void icon_draw_rect_input(
    float x, float y, int w, int h, int icon_id, float aspect, float alpha, bool inverted);

/* `resources.cc` */

void resources_init();
void resources_free();

/* `interface_layout.cc` */

void layout_add_but(Layout *layout, Button *but);
void layout_remove_but(Layout *layout, const Button *but);
/**
 * \return true if the button was successfully replaced.
 */
bool layout_replace_but_ptr(Layout *layout, const void *old_but_ptr, Button *new_but);

/**
 * \note \a but type must be a ButtonType::SearchMenu. If the property is a string property and
 * does not contains the #PROP_STRING_SEARCH_SUPPORTED flag or if the search pointer-property pair
 * is not provided/found it will disable the button.
 */
void button_configure_search(Button *but,
                             PointerRNA *ptr,
                             PropertyRNA *prop,
                             PointerRNA *searchptr,
                             PropertyRNA *searchprop,
                             PropertyRNA *item_searchprop,
                             bool results_are_suggestions);
/**
 * Check all buttons defined in this layout,
 * and set any button flagged as BUT_LIST_ITEM as active/selected.
 * Needed to handle correctly text colors of active (selected) list item.
 */
void layout_list_set_labels_active(Layout *layout);
/* menu callback */
void item_menutype_func(bContext *C, Layout *layout, void *arg_mt);
void item_paneltype_func(bContext *C, Layout *layout, void *arg_pt);

/* `interface_button_group.cc` */

/**
 * Every function that adds a set of buttons must create another group,
 * then #ui_def_but adds buttons to the current group (the last).
 */
void block_new_button_group(Block *block, ButtonGroupFlag flag);
void button_group_add_but(Block *block, Button *but);
void button_group_replace_but_ptr(Block *block, const Button *old_but_ptr, Button *new_but);

/* `interface_drag.cc` */

void button_drag_free(Button *but);
bool button_drag_is_draggable(const Button *but);
void button_drag_start(bContext *C, Button *but);

/* `interface_align.cc` */

bool button_can_align(const Button *but) ATTR_WARN_UNUSED_RESULT;
int button_align_opposite_to_area_align_get(const ARegion *region) ATTR_WARN_UNUSED_RESULT;
/**
 * Compute the alignment of all 'align groups' of buttons in given block.
 *
 * This is using an order-independent algorithm,
 * i.e. alignment of buttons should be OK regardless of order in which
 * they are added to the block.
 */
void block_align_calc(Block *block, const ARegion *region);

/* `interface_anim.cc` */

void button_anim_flag(Button *but, const AnimationEvalContext *anim_eval_context);
void button_anim_copy_driver(bContext *C);
void button_anim_paste_driver(bContext *C);
/**
 * \a str can be NULL to only perform check if \a but has an expression at all.
 * \return if button has an expression.
 */
bool button_anim_expression_get(Button *but, char *str, size_t str_maxncpy);
bool button_anim_expression_set(Button *but, const char *str);
/**
 * Create new expression for button (i.e. a "scripted driver"), if it can be created.
 */
bool button_anim_expression_create(Button *but, const char *str);
void button_anim_autokey(bContext *C, Button *but, Scene *scene, float cfra);

void button_anim_decorate_cb(bContext *C, void *arg_but, void *arg_dummy);
void button_anim_decorate_update_from_flag(ButtonDecorator *but);

/* `interface_query.cc` */

bool button_is_editable(const Button *but) ATTR_WARN_UNUSED_RESULT;
bool button_is_editable_as_text(const Button *but) ATTR_WARN_UNUSED_RESULT;
bool button_is_toggle(const Button *but) ATTR_WARN_UNUSED_RESULT;
/**
 * Can we mouse over the button or is it hidden/disabled/layout.
 * \note ctrl is kind of a hack currently,
 * so that non-embossed ButtonType::Text button behaves as a label when ctrl is not pressed.
 */
bool button_is_interactive_ex(const Button *but, const bool labeledit, const bool for_tooltip);
bool button_is_interactive(const Button *but, bool labeledit) ATTR_WARN_UNUSED_RESULT;
bool button_is_popover_once_compat(const Button *but) ATTR_WARN_UNUSED_RESULT;
bool button_has_array_value(const Button *but) ATTR_WARN_UNUSED_RESULT;
int button_icon(const Button *but);
void button_pie_dir(RadialDirection dir, float vec[2]);

bool button_is_cursor_warp(const Button *but) ATTR_WARN_UNUSED_RESULT;

bool button_contains_pt(const Button *but, float mx, float my) ATTR_WARN_UNUSED_RESULT;
bool button_contains_rect(const Button *but, const rctf *rect);
bool ui_but_contains_point_px_icon(const Button *but,
                                   ARegion *region,
                                   const wmEvent *event) ATTR_WARN_UNUSED_RESULT;
bool button_contains_point_px(const Button *but, const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2, 3) ATTR_WARN_UNUSED_RESULT;

Button *ui_list_find_mouse_over(const ARegion *region,
                                const wmEvent *event) ATTR_WARN_UNUSED_RESULT;
Button *list_row_find_mouse_over(const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
Button *list_row_find_index(const ARegion *region,
                            int index,
                            Button *listbox) ATTR_WARN_UNUSED_RESULT;
Button *view_item_find_mouse_over(const ARegion *region, const int xy[2]) ATTR_NONNULL(1, 2);
Button *view_item_find_active(const ARegion *region);
Button *view_item_find_search_highlight(const ARegion *region);

using ButtonFindPollFn = bool (*)(const Button *but, const void *customdata);
/**
 * x and y are only used in case event is NULL.
 */
Button *button_find_mouse_over_ex(const ARegion *region,
                                  const int xy[2],
                                  bool labeledit,
                                  bool for_tooltip,
                                  const ButtonFindPollFn find_poll,
                                  const void *find_custom_data)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
Button *button_find_rect_over(const ARegion *region, const rcti *rect_px) ATTR_WARN_UNUSED_RESULT;

Button *list_find_mouse_over_ex(const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

bool but_contains_password(const Button *but) ATTR_WARN_UNUSED_RESULT;

StringRef button_drawstr_without_sep_char(const Button *but) ATTR_NONNULL();
size_t button_drawstr_len_without_sep_char(const Button *but);
size_t button_tip_len_only_first_line(const Button *but);

Button *button_prev(Button *but) ATTR_WARN_UNUSED_RESULT;
Button *button_next(Button *but) ATTR_WARN_UNUSED_RESULT;
Button *button_first(Block *block) ATTR_WARN_UNUSED_RESULT;
Button *button_last(Block *block) ATTR_WARN_UNUSED_RESULT;

Button *block_active_but_get(const Block *block);
bool block_is_menu(const Block *block) ATTR_WARN_UNUSED_RESULT;
bool block_is_popover(const Block *block) ATTR_WARN_UNUSED_RESULT;
bool block_is_pie_menu(const Block *block) ATTR_WARN_UNUSED_RESULT;
bool block_is_popup_any(const Block *block) ATTR_WARN_UNUSED_RESULT;

Block *block_find_mouse_over_ex(const ARegion *region, const int xy[2], bool only_clip)
    ATTR_NONNULL(1, 2);
Block *block_find_mouse_over(const ARegion *region, const wmEvent *event, bool only_clip);

Button *region_find_first_but_test_flag(ARegion *region, int flag_include, int flag_exclude);
Button *region_find_active_but(ARegion *region) ATTR_WARN_UNUSED_RESULT;
bool region_contains_point_px(const ARegion *region, const int xy[2])
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
bool region_contains_rect_px(const ARegion *region, const rcti *rect_px);

/**
 * Check if the cursor is over any popups.
 */
ARegion *screen_region_find_mouse_over_ex(bScreen *screen, const int xy[2]) ATTR_NONNULL(1, 2);
ARegion *screen_region_find_mouse_over(bScreen *screen, const wmEvent *event);

/* `interface_context_menu.cc` */

bool popup_context_menu_for_button(bContext *C, Button *but, const wmEvent *event);
/**
 * menu to show when right clicking on the panel header
 */
void popup_context_menu_for_panel(bContext *C, ARegion *region, Panel *panel);

/* `eyedroppers/interface_eyedropper.cc` */

wmKeyMap *eyedropper_modal_keymap(wmKeyConfig *keyconf);
wmKeyMap *eyedropper_colorband_modal_keymap(wmKeyConfig *keyconf);

/* `eyedroppers/eyedropper_color.cc` */

void UI_OT_eyedropper_color(wmOperatorType *ot);

/* `interface_eyedropper_colorband.cc` */

void UI_OT_eyedropper_colorramp(wmOperatorType *ot);
void UI_OT_eyedropper_colorramp_point(wmOperatorType *ot);

void UI_OT_eyedropper_bone(wmOperatorType *ot);

/* `eyedroppers/eyedropper_datablock.cc` */

void UI_OT_eyedropper_id(wmOperatorType *ot);

/* `eyedroppers/eyedropper_depth.cc` */

void UI_OT_eyedropper_depth(wmOperatorType *ot);

/* `eyedroppers/eyedropper_driver.cc` */

void UI_OT_eyedropper_driver(wmOperatorType *ot);

/* `eyedroppers/eyedropper_grease_pencil_colorr.cc` */

void UI_OT_eyedropper_grease_pencil_color(wmOperatorType *ot);

/* `templates/interface_template_asset_shelf_popover.cc` */
std::optional<StringRefNull> asset_shelf_idname_from_button_context(const Button *but);

/**
 * For use with #ui_rna_collection_search_update_fn.
 */
struct RNACollectionSearch {
  PointerRNA target_ptr;
  PropertyRNA *target_prop;

  PointerRNA search_ptr;
  PropertyRNA *search_prop;
  PropertyRNA *item_search_prop;

  Button *search_but;
  /** Let `UI_butstore_*` API update search_but pointer above over redraws. */
  ButStore *butstore;
  /** Block has to be stored for freeing but-store (#Button::block doesn't work with undo). */
  Block *butstore_block;
};
void rna_collection_search_update_fn(
    const bContext *C, void *arg, const char *str, SearchItems *items, bool is_first);

/* `interface_ops.cc` */

bool jump_to_target_button_poll(bContext *C);

/* `interface_query.cc` */

void interface_tag_script_reload_queries();

/* `views/interface_view.cc` */

void block_free_views(Block *block);
void block_views_end(ARegion *region, const Block *block);
void block_view_persistent_state_restore(const ARegion &region,
                                         const Block &block,
                                         AbstractView &view);
void block_views_listen(const Block *block, const wmRegionListenerParams *listener_params);
void block_views_draw_overlays(const ARegion *region, const Block *block);
AbstractView *block_view_find_matching_in_old_block(const Block &new_block,
                                                    const AbstractView &new_view);

ButtonViewItem *block_view_find_matching_view_item_but_in_old_block(
    const Block &new_block, const AbstractViewItem &new_item);

/* `views/abstract_view_item.cc` */

void view_item_swap_button_pointers(AbstractViewItem &a, AbstractViewItem &b);

/* `views/interface_templates.cc` */

uiListType *UI_UL_cache_file_layers();

ID *template_id_liboverride_hierarchy_make(
    bContext *C, Main *bmain, ID *owner_id, ID *id, const char **r_undo_push_label);

/**
 * Functions in this namespace are only exposed for unit testing purposes, and
 * should not be used outside of the files where they are defined.
 */
namespace internal {

/**
 * Get the driver(s) of the given property.
 *
 * \note intended to be used in conjunction with `paste_property_drivers()` below.
 *
 * \param ptr: The RNA pointer of the property.
 * \param prop: The property RNA of the property.
 * \param get_all: Whether to get all drivers of an array property, or just the
 * one specified by `index`.  Ignored if the property is not an array property.
 * \param index: Which element of an array property to get.  Ignored if `get_all`
 * is true or if the property is not an array properly.
 * \param r_is_array_prop: Output parameter, that stores whether the passed
 * property is an array property or not.
 *
 * \returns A vector of pointers to the drivers of the property.  It will be
 * zero-sized if no drivers were fetched (e.g. if the property had no drivers).
 * Otherwise the vector will be the size of the underlying property (e.g. 4 for
 * an array property with 4 elements, 1 for a non-array property).  For array
 * properties, elements without drivers will be null.
 */
Vector<FCurve *> get_property_drivers(
    PointerRNA *ptr, PropertyRNA *prop, bool get_all, int index, bool *r_is_array_prop);

/**
 * Paste the drivers from `src_drivers` to the destination property.
 *
 * This function can be used for pasting drivers for all elements of an array
 * property, just some elements of an array property, or a single driver for a
 * non-array property.
 *
 * \note intended to be used in conjunction with `get_property_drivers()` above.
 * The destination property should have the same type and (if an array property)
 * length as the source property passed to `get_property_drivers()`.
 *
 * \param src_drivers: The span of drivers to paste.  If `is_array_prop` is
 * false, this must be a single element.  If `is_array_prop` is true then this
 * should have the same length as the destination array property.  Nullptr
 * elements are skipped when pasting.
 * \param is_array_prop: Whether `src_drivers` are drivers for the elements
 * of an array property.
 * \param dst_ptr: The RNA pointer for the destination property.
 * \param dist_prop: The destination property RNA.
 *
 * \returns The number of successfully pasted drivers.
 */
int paste_property_drivers(Span<FCurve *> src_drivers,
                           bool is_array_prop,
                           PointerRNA *dst_ptr,
                           PropertyRNA *dst_prop);

}  // namespace internal

}  // namespace blender::ui
