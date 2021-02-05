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
 * \ingroup edinterface
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_rect.h"

#include "DNA_listBase.h"
#include "RNA_types.h"
#include "UI_interface.h"
#include "UI_resources.h"

struct ARegion;
struct AnimationEvalContext;
struct CurveMapping;
struct CurveProfile;
struct ID;
struct ImBuf;
struct Scene;
struct bContext;
struct bContextStore;
struct uiHandleButtonData;
struct uiLayout;
struct uiStyle;
struct uiUndoStack_Text;
struct uiWidgetColors;
struct wmEvent;
struct wmKeyConfig;
struct wmOperatorType;
struct wmTimer;

/* ****************** general defines ************** */

#define RNA_NO_INDEX -1
#define RNA_ENUM_VALUE -2

#define UI_MENU_PADDING (int)(0.2f * UI_UNIT_Y)

#define UI_MENU_WIDTH_MIN (UI_UNIT_Y * 9)
/* some extra padding added to menus containing submenu icons */
#define UI_MENU_SUBMENU_PADDING (6 * UI_DPI_FAC)

/* menu scrolling */
#define UI_MENU_SCROLL_ARROW (12 * UI_DPI_FAC)
#define UI_MENU_SCROLL_MOUSE (UI_MENU_SCROLL_ARROW + 2 * UI_DPI_FAC)
#define UI_MENU_SCROLL_PAD (4 * UI_DPI_FAC)

/* panel limits */
#define UI_PANEL_MINX 100
#define UI_PANEL_MINY 70

/* popover width (multiplied by 'U.widget_unit') */
#define UI_POPOVER_WIDTH_UNITS 10

/* uiBut->flag */
enum {
  UI_SELECT = (1 << 0),   /* use when the button is pressed */
  UI_SCROLLED = (1 << 1), /* temp hidden, scrolled away */
  UI_ACTIVE = (1 << 2),
  UI_HAS_ICON = (1 << 3),
  UI_HIDDEN = (1 << 4),
  UI_SELECT_DRAW = (1 << 5), /* Display selected, doesn't impact interaction. */
  /** Property search filter is active and the button does not match. */
  UI_SEARCH_FILTER_NO_MATCH = (1 << 12),
  /* warn: rest of uiBut->flag in UI_interface.h */
};

/* uiBut->dragflag */
enum {
  UI_BUT_DRAGPOIN_FREE = (1 << 0),
};

/* but->pie_dir */
typedef enum RadialDirection {
  UI_RADIAL_NONE = -1,
  UI_RADIAL_N = 0,
  UI_RADIAL_NE = 1,
  UI_RADIAL_E = 2,
  UI_RADIAL_SE = 3,
  UI_RADIAL_S = 4,
  UI_RADIAL_SW = 5,
  UI_RADIAL_W = 6,
  UI_RADIAL_NW = 7,
} RadialDirection;

extern const char ui_radial_dir_order[8];
extern const char ui_radial_dir_to_numpad[8];
extern const short ui_radial_dir_to_angle[8];

/* internal panel drawing defines */
#define PNL_HEADER (UI_UNIT_Y * 1.2) /* 24 default */

/* bit button defines */
/* Bit operations */
#define UI_BITBUT_TEST(a, b) (((a) & (1 << (b))) != 0)
#define UI_BITBUT_VALUE_TOGGLED(a, b) ((a) ^ (1 << (b)))
#define UI_BITBUT_VALUE_ENABLED(a, b) ((a) | (1 << (b)))
#define UI_BITBUT_VALUE_DISABLED(a, b) ((a) & ~(1 << (b)))

/* bit-row */
#define UI_BITBUT_ROW(min, max) \
  (((max) >= 31 ? 0xFFFFFFFF : (1 << ((max) + 1)) - 1) - ((min) ? ((1 << (min)) - 1) : 0))

/* split numbuts by ':' and align l/r */
#define USE_NUMBUTS_LR_ALIGN

/* Use new 'align' computation code. */
#define USE_UIBUT_SPATIAL_ALIGN

/* PieMenuData->flags */
enum {
  /** pie menu item collision is detected at 90 degrees */
  UI_PIE_DEGREES_RANGE_LARGE = (1 << 0),
  /** use initial center of pie menu to calculate direction */
  UI_PIE_INITIAL_DIRECTION = (1 << 1),
  /** pie menu is drag style */
  UI_PIE_DRAG_STYLE = (1 << 2),
  /** mouse not far enough from center position  */
  UI_PIE_INVALID_DIR = (1 << 3),
  /** pie menu changed to click style, click to confirm  */
  UI_PIE_CLICK_STYLE = (1 << 4),
  /** pie animation finished, do not calculate any more motion  */
  UI_PIE_ANIMATION_FINISHED = (1 << 5),
  /** pie gesture selection has been done, now wait for mouse motion to end */
  UI_PIE_GESTURE_END_WAIT = (1 << 6),
};

#define PIE_CLICK_THRESHOLD_SQ 50.0f

/* max amount of items a radial menu (pie menu) can contain */
#define PIE_MAX_ITEMS 8

struct uiBut {
  struct uiBut *next, *prev;

  /* Pointer back to the layout item holding this button. */
  uiLayout *layout;
  int flag, drawflag;
  eButType type;
  eButPointerType pointype;
  short bit, bitnr, retval, strwidth, alignnr;
  short ofs, pos, selsta, selend;

  char *str;
  char strdata[UI_MAX_NAME_STR];
  char drawstr[UI_MAX_DRAW_STR];

  rctf rect; /* block relative coords */

  char *poin;
  float hardmin, hardmax, softmin, softmax;

  /* both these values use depends on the button type
   * (polymorphic struct or union would be nicer for this stuff) */

  /**
   * For #uiBut.type:
   * - UI_BTYPE_LABEL:        Use `(a1 == 1.0f)` to use a2 as a blending factor (imaginative!).
   * - UI_BTYPE_SCROLL:       Use as scroll size.
   * - UI_BTYPE_SEARCH_MENU:  Use as number or rows.
   */
  float a1;

  /**
   * For #uiBut.type:
   * - UI_BTYPE_HSVCIRCLE:    Use to store the luminosity.
   * - UI_BTYPE_LABEL:        If `(a1 == 1.0f)` use a2 as a blending factor.
   * - UI_BTYPE_SEARCH_MENU:  Use as number or columns.
   */
  float a2;

  uchar col[4];

  uiButHandleFunc func;
  void *func_arg1;
  void *func_arg2;

  uiButHandleNFunc funcN;
  void *func_argN;

  struct bContextStore *context;

  uiButCompleteFunc autocomplete_func;
  void *autofunc_arg;

  uiButHandleRenameFunc rename_func;
  void *rename_arg1;
  void *rename_orig;

  /** Run an action when holding the button down. */
  uiButHandleHoldFunc hold_func;
  void *hold_argN;

  const char *tip;
  uiButToolTipFunc tip_func;
  void *tip_argN;

  /** info on why button is disabled, displayed in tooltip */
  const char *disabled_info;

  BIFIconID icon;
  /** Copied from the #uiBlock.emboss */
  eUIEmbossType emboss;
  /** direction in a pie menu, used for collision detection (RadialDirection) */
  signed char pie_dir;
  /** could be made into a single flag */
  bool changed;
  /** so buttons can support unit systems which are not RNA */
  uchar unit_type;
  short modifier_key;
  short iconadd;

  /* UI_BTYPE_BLOCK data */
  uiBlockCreateFunc block_create_func;

  /* UI_BTYPE_PULLDOWN/UI_BTYPE_MENU data */
  uiMenuCreateFunc menu_create_func;

  uiMenuStepFunc menu_step_func;

  /* RNA data */
  struct PointerRNA rnapoin;
  struct PropertyRNA *rnaprop;
  int rnaindex;

  /* Operator data */
  struct wmOperatorType *optype;
  struct PointerRNA *opptr;
  short opcontext;
  uchar menu_key; /* 'a'-'z', always lower case */

  ListBase extra_op_icons; /* uiButExtraOpIcon */

  /* Draggable data, type is WM_DRAG_... */
  char dragtype;
  short dragflag;
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

  uiButPushedStateFunc pushed_state_func;
  void *pushed_state_arg;

  /* pointer back */
  uiBlock *block;
};

/** Derived struct for #UI_BTYPE_NUM */
typedef struct uiButNumber {
  uiBut but;

  float step_size;
  float precision;
} uiButNumber;

/** Derived struct for #UI_BTYPE_COLOR */
typedef struct uiButColor {
  uiBut but;

  bool is_pallete_color;
  int palette_color_index;
} uiButColor;

/** Derived struct for #UI_BTYPE_TAB */
typedef struct uiButTab {
  uiBut but;
  struct MenuType *menu;
} uiButTab;

/** Derived struct for #UI_BTYPE_SEARCH_MENU */
typedef struct uiButSearch {
  uiBut but;

  uiButSearchCreateFn popup_create_fn;
  uiButSearchUpdateFn items_update_fn;
  void *item_active;

  void *arg;
  uiButSearchArgFreeFn arg_free_fn;

  uiButSearchContextMenuFn item_context_menu_fn;
  uiButSearchTooltipFn item_tooltip_fn;

  const char *item_sep_string;

  struct PointerRNA rnasearchpoin;
  struct PropertyRNA *rnasearchprop;
} uiButSearch;

/** Derived struct for #UI_BTYPE_DECORATOR */
typedef struct uiButDecorator {
  uiBut but;

  struct PointerRNA rnapoin;
  struct PropertyRNA *rnaprop;
  int rnaindex;
} uiButDecorator;

/** Derived struct for #UI_BTYPE_PROGRESS_BAR. */
typedef struct uiButProgressbar {
  uiBut but;

  /* 0..1 range */
  float progress;
} uiButProgressbar;

/** Derived struct for #UI_BTYPE_HSVCUBE. */
typedef struct uiButHSVCube {
  uiBut but;

  eButGradientType gradient_type;
} uiButHSVCube;

/** Derived struct for #UI_BTYPE_COLORBAND. */
typedef struct uiButColorBand {
  uiBut but;

  struct ColorBand *edit_coba;
} uiButColorBand;

/** Derived struct for #UI_BTYPE_CURVEPROFILE. */
typedef struct uiButCurveProfile {
  uiBut but;

  struct CurveProfile *edit_profile;
} uiButCurveProfile;

/** Derived struct for #UI_BTYPE_CURVE. */
typedef struct uiButCurveMapping {
  uiBut but;

  struct CurveMapping *edit_cumap;
  eButGradientType gradient_type;
} uiButCurveMapping;

/**
 * Additional, superimposed icon for a button, invoking an operator.
 */
typedef struct uiButExtraOpIcon {
  struct uiButExtraOpIcon *next, *prev;

  BIFIconID icon;
  struct wmOperatorCallParams *optype_params;

  bool highlighted;
} uiButExtraOpIcon;

typedef struct ColorPicker {
  struct ColorPicker *next, *prev;

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
} ColorPicker;

typedef struct ColorPickerData {
  ListBase list;
} ColorPickerData;

struct PieMenuData {
  /** store title and icon to allow access when pie levels are created */
  const char *title;
  int icon;

  float pie_dir[2];
  float pie_center_init[2];
  float pie_center_spawned[2];
  float last_pos[2];
  double duration_gesture;
  int flags;
  /** initial event used to fire the pie menu, store here so we can query for release */
  int event;
  float alphafac;
};

/* uiBlock.content_hints */
enum eBlockContentHints {
  /** In a menu block, if there is a single sub-menu button, we add some
   * padding to the right to put nicely aligned triangle icons there. */
  UI_BLOCK_CONTAINS_SUBMENU_BUT = (1 << 0),
};

/**
 * A group of button references, used by property search to keep track of sets of buttons that
 * should be searched together. For example, in property split layouts number buttons and their
 * labels (and even their decorators) are separate buttons, but they must be searched and
 * highlighted together.
 */
typedef struct uiButtonGroup {
  void *next, *prev;
  ListBase buttons; /* #LinkData with #uiBut data field. */
  short flag;
} uiButtonGroup;

/* #uiButtonGroup.flag. */
typedef enum uiButtonGroupFlag {
  /** While this flag is set, don't create new button groups for layout item calls. */
  UI_BUTTON_GROUP_LOCK = (1 << 0),
  /** The buttons in this group are inside a panel header. */
  UI_BUTTON_GROUP_PANEL_HEADER = (1 << 1),
} uiButtonGroupFlag;

struct uiBlock {
  uiBlock *next, *prev;

  ListBase buttons;
  struct Panel *panel;
  uiBlock *oldblock;

  ListBase butstore; /* UI_butstore_* runtime function */

  ListBase button_groups; /* #uiButtonGroup. */

  ListBase layouts;
  struct uiLayout *curlayout;

  ListBase contexts;

  char name[UI_MAX_NAME_STR];

  float winmat[4][4];

  rctf rect;
  float aspect;

  uint puphash; /* popup menu hash for memory */

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
  /** Hints about the buttons of this block. Used to avoid iterating over
   * buttons to find out if some criteria is met by any. Instead, check this
   * criteria when adding the button and set a flag here if it's met. */
  short content_hints; /* eBlockContentHints */

  char direction;
  /** UI_BLOCK_THEME_STYLE_* */
  char theme_style;
  /** Copied to #uiBut.emboss */
  eUIEmbossType emboss;
  bool auto_open;
  char _pad[5];
  double auto_open_last;

  const char *lockstr;

  char lock;
  /** to keep blocks while drawing and free them afterwards */
  char active;
  /** to avoid tooltip after click */
  char tooltipdisabled;
  /** UI_block_end done? */
  char endblock;

  /** for doing delayed */
  eBlockBoundsCalc bounds_type;
  /** Offset to use when calculating bounds (in pixels). */
  int bounds_offset[2];
  /** for doing delayed */
  int bounds, minbounds;

  /** pull-downs, to detect outside, can differ per case how it is created. */
  rctf safety;
  /** uiSafetyRct list */
  ListBase saferct;

  uiPopupBlockHandle *handle; /* handle */

  /** use so presets can find the operator,
   * across menus and from nested popups which fail for operator context. */
  struct wmOperator *ui_operator;

  /** XXX hack for dynamic operator enums */
  void *evil_C;

  /** unit system, used a lot for numeric buttons so include here
   * rather than fetching through the scene every time. */
  struct UnitSettings *unit;
  /** \note only accessed by color picker templates. */
  ColorPickerData color_pickers;

  bool is_color_gamma_picker; /* Block for color picker with gamma baked in. */

  /** display device name used to display this block,
   * used by color widgets to transform colors from/to scene linear
   */
  char display_device[64];

  struct PieMenuData pie_data;
};

typedef struct uiSafetyRct {
  struct uiSafetyRct *next, *prev;
  rctf parent;
  rctf safety;
} uiSafetyRct;

/* interface.c */

void ui_fontscale(short *points, float aspect);

extern void ui_block_to_window_fl(const struct ARegion *region,
                                  uiBlock *block,
                                  float *x,
                                  float *y);
extern void ui_block_to_window(const struct ARegion *region, uiBlock *block, int *x, int *y);
extern void ui_block_to_window_rctf(const struct ARegion *region,
                                    uiBlock *block,
                                    rctf *rct_dst,
                                    const rctf *rct_src);
extern float ui_block_to_window_scale(const struct ARegion *region, uiBlock *block);
extern void ui_window_to_block_fl(const struct ARegion *region,
                                  uiBlock *block,
                                  float *x,
                                  float *y);
extern void ui_window_to_block(const struct ARegion *region, uiBlock *block, int *x, int *y);
extern void ui_window_to_block_rctf(const struct ARegion *region,
                                    uiBlock *block,
                                    rctf *rct_dst,
                                    const rctf *rct_src);
extern void ui_window_to_region(const struct ARegion *region, int *x, int *y);
extern void ui_window_to_region_rcti(const struct ARegion *region,
                                     rcti *rect_dst,
                                     const rcti *rct_src);
extern void ui_region_to_window(const struct ARegion *region, int *x, int *y);
extern void ui_region_winrct_get_no_margin(const struct ARegion *region, struct rcti *r_rect);

uiBut *ui_but_change_type(uiBut *but, eButType new_type);

extern double ui_but_value_get(uiBut *but);
extern void ui_but_value_set(uiBut *but, double value);
extern void ui_but_hsv_set(uiBut *but);
extern void ui_but_v3_get(uiBut *but, float vec[3]);
extern void ui_but_v3_set(uiBut *but, const float vec[3]);

extern void ui_hsvcircle_vals_from_pos(
    const rcti *rect, const float mx, const float my, float *r_val_rad, float *r_val_dist);
extern void ui_hsvcircle_pos_from_vals(
    const ColorPicker *cpicker, const rcti *rect, const float *hsv, float *xpos, float *ypos);
extern void ui_hsvcube_pos_from_vals(
    const struct uiButHSVCube *hsv_but, const rcti *rect, const float *hsv, float *xp, float *yp);

extern void ui_but_string_get_ex(uiBut *but,
                                 char *str,
                                 const size_t maxlen,
                                 const int float_precision,
                                 const bool use_exp_float,
                                 bool *r_use_exp_float) ATTR_NONNULL(1, 2);
extern void ui_but_string_get(uiBut *but, char *str, const size_t maxlen) ATTR_NONNULL();
extern char *ui_but_string_get_dynamic(uiBut *but, int *r_str_size);
extern void ui_but_convert_to_unit_alt_name(uiBut *but, char *str, size_t maxlen) ATTR_NONNULL();
extern bool ui_but_string_set(struct bContext *C, uiBut *but, const char *str) ATTR_NONNULL();
extern bool ui_but_string_eval_number(struct bContext *C,
                                      const uiBut *but,
                                      const char *str,
                                      double *value) ATTR_NONNULL();
extern int ui_but_string_get_max_length(uiBut *but);
/* Clear & exit the active button's string. */
extern void ui_but_active_string_clear_and_exit(struct bContext *C, uiBut *but) ATTR_NONNULL();
extern uiBut *ui_but_drag_multi_edit_get(uiBut *but);

void ui_def_but_icon(uiBut *but, const int icon, const int flag);
void ui_def_but_icon_clear(uiBut *but);

void ui_but_extra_operator_icons_free(uiBut *but);

extern void ui_but_rna_menu_convert_to_panel_type(struct uiBut *but, const char *panel_type);
extern void ui_but_rna_menu_convert_to_menu_type(struct uiBut *but, const char *menu_type);
extern bool ui_but_menu_draw_as_popover(const uiBut *but);

void ui_but_range_set_hard(uiBut *but);
void ui_but_range_set_soft(uiBut *but);

extern void ui_but_update(uiBut *but);
extern void ui_but_update_edited(uiBut *but);
extern bool ui_but_is_float(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_but_is_bool(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_but_is_unit(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_but_is_compatible(const uiBut *but_a, const uiBut *but_b) ATTR_WARN_UNUSED_RESULT;
extern bool ui_but_is_rna_valid(uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_but_supports_cycling(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

extern int ui_but_is_pushed_ex(uiBut *but, double *value) ATTR_WARN_UNUSED_RESULT;
extern int ui_but_is_pushed(uiBut *but) ATTR_WARN_UNUSED_RESULT;

void ui_but_override_flag(struct Main *bmain, uiBut *but);

extern void ui_block_bounds_calc(uiBlock *block);

extern struct ColorManagedDisplay *ui_block_cm_display_get(uiBlock *block);
void ui_block_cm_to_display_space_v3(uiBlock *block, float pixel[3]);

/* interface_regions.c */

struct uiKeyNavLock {
  /* Set when we're using key-input. */
  bool is_keynav;
  /* only used to check if we've moved the cursor */
  int event_xy[2];
};

typedef uiBlock *(*uiBlockHandleCreateFunc)(struct bContext *C,
                                            struct uiPopupBlockHandle *handle,
                                            void *arg1);

struct uiPopupBlockCreate {
  uiBlockCreateFunc create_func;
  uiBlockHandleCreateFunc handle_create_func;
  void *arg;
  void (*arg_free)(void *arg);

  int event_xy[2];

  /* when popup is initialized from a button */
  struct ARegion *butregion;
  uiBut *but;
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
  /* true if we can re-create the popup using 'popup_create_vars' */
  bool can_refresh;
  bool refresh;

  struct wmTimer *scrolltimer;
  float scrolloffset;

  struct uiKeyNavLock keynav_state;

  /* for operator popups */
  struct wmOperator *popup_op;
  struct ScrArea *ctx_area;
  struct ARegion *ctx_region;

  /* return values */
  int butretval;
  int menuretval;
  int retvalue;
  float retvec[4];

  /* menu direction */
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
};

/* -------------------------------------------------------------------- */
/* interface_region_*.c */

/* interface_region_tooltip.c */
/* exposed as public API in UI_interface.h */

/* interface_region_color_picker.c */
void ui_color_picker_rgb_to_hsv_compat(const float rgb[3], float r_cp[3]);
void ui_color_picker_rgb_to_hsv(const float rgb[3], float r_cp[3]);
void ui_color_picker_hsv_to_rgb(const float r_cp[3], float rgb[3]);

bool ui_but_is_color_gamma(uiBut *but);

void ui_scene_linear_to_perceptual_space(uiBut *but, float rgb[3]);
void ui_perceptual_to_scene_linear_space(uiBut *but, float rgb[3]);

uiBlock *ui_block_func_COLOR(struct bContext *C, uiPopupBlockHandle *handle, void *arg_but);
ColorPicker *ui_block_colorpicker_create(struct uiBlock *block);

/* interface_region_search.c */
/* Search-box for string button. */
struct ARegion *ui_searchbox_create_generic(struct bContext *C,
                                            struct ARegion *butregion,
                                            uiButSearch *search_but);
struct ARegion *ui_searchbox_create_operator(struct bContext *C,
                                             struct ARegion *butregion,
                                             uiButSearch *search_but);
struct ARegion *ui_searchbox_create_menu(struct bContext *C,
                                         struct ARegion *butregion,
                                         uiButSearch *search_but);

bool ui_searchbox_inside(struct ARegion *region, int x, int y);
int ui_searchbox_find_index(struct ARegion *region, const char *name);
void ui_searchbox_update(struct bContext *C, struct ARegion *region, uiBut *but, const bool reset);
int ui_searchbox_autocomplete(struct bContext *C, struct ARegion *region, uiBut *but, char *str);
bool ui_searchbox_event(struct bContext *C,
                        struct ARegion *region,
                        uiBut *but,
                        struct ARegion *butregion,
                        const struct wmEvent *event);
bool ui_searchbox_apply(uiBut *but, struct ARegion *region);
void ui_searchbox_free(struct bContext *C, struct ARegion *region);
void ui_but_search_refresh(uiButSearch *but);

/* interface_region_menu_popup.c */
int ui_but_menu_step(uiBut *but, int direction);
bool ui_but_menu_step_poll(const uiBut *but);
uiBut *ui_popup_menu_memory_get(struct uiBlock *block);
void ui_popup_menu_memory_set(uiBlock *block, struct uiBut *but);

uiBlock *ui_popup_block_refresh(struct bContext *C,
                                uiPopupBlockHandle *handle,
                                struct ARegion *butregion,
                                uiBut *but);

uiPopupBlockHandle *ui_popup_block_create(struct bContext *C,
                                          struct ARegion *butregion,
                                          uiBut *but,
                                          uiBlockCreateFunc create_func,
                                          uiBlockHandleCreateFunc handle_create_func,
                                          void *arg,
                                          void (*arg_free)(void *arg));
uiPopupBlockHandle *ui_popup_menu_create(struct bContext *C,
                                         struct ARegion *butregion,
                                         uiBut *but,
                                         uiMenuCreateFunc menu_func,
                                         void *arg);

/* interface_region_popover.c */
uiPopupBlockHandle *ui_popover_panel_create(struct bContext *C,
                                            struct ARegion *butregion,
                                            uiBut *but,
                                            uiMenuCreateFunc menu_func,
                                            void *arg);

/* interface_region_menu_pie.c */
void ui_pie_menu_level_create(uiBlock *block,
                              struct wmOperatorType *ot,
                              const char *propname,
                              struct IDProperty *properties,
                              const EnumPropertyItem *items,
                              int totitem,
                              int context,
                              int flag);

/* interface_region_popup.c */
void ui_popup_translate(struct ARegion *region, const int mdiff[2]);
void ui_popup_block_free(struct bContext *C, uiPopupBlockHandle *handle);
void ui_popup_block_scrolltest(struct uiBlock *block);

/* end interface_region_*.c */

/* interface_panel.c */
extern int ui_handler_panel_region(struct bContext *C,
                                   const struct wmEvent *event,
                                   struct ARegion *region,
                                   const uiBut *active_but);
extern void ui_draw_aligned_panel(const struct uiStyle *style,
                                  const uiBlock *block,
                                  const rcti *rect,
                                  const bool show_pin,
                                  const bool show_background,
                                  const bool region_search_filter_active);
void ui_panel_tag_search_filter_match(struct Panel *panel);

/* interface_draw.c */
extern void ui_draw_dropshadow(
    const rctf *rct, float radius, float aspect, float alpha, int select);

void ui_draw_gradient(const rcti *rect,
                      const float hsv[3],
                      const eButGradientType type,
                      const float alpha);

void ui_draw_but_TAB_outline(const rcti *rect,
                             float rad,
                             uchar highlight[3],
                             uchar highlight_fade[3]);
void ui_draw_but_HISTOGRAM(struct ARegion *region,
                           uiBut *but,
                           const struct uiWidgetColors *wcol,
                           const rcti *rect);
void ui_draw_but_WAVEFORM(struct ARegion *region,
                          uiBut *but,
                          const struct uiWidgetColors *wcol,
                          const rcti *rect);
void ui_draw_but_VECTORSCOPE(struct ARegion *region,
                             uiBut *but,
                             const struct uiWidgetColors *wcol,
                             const rcti *rect);
void ui_draw_but_COLORBAND(uiBut *but, const struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_UNITVEC(uiBut *but, const struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_CURVE(struct ARegion *region,
                       uiBut *but,
                       const struct uiWidgetColors *wcol,
                       const rcti *rect);
void ui_draw_but_CURVEPROFILE(struct ARegion *region,
                              uiBut *but,
                              const struct uiWidgetColors *wcol,
                              const rcti *rect);
void ui_draw_but_IMAGE(struct ARegion *region,
                       uiBut *but,
                       const struct uiWidgetColors *wcol,
                       const rcti *rect);
void ui_draw_but_TRACKPREVIEW(struct ARegion *region,
                              uiBut *but,
                              const struct uiWidgetColors *wcol,
                              const rcti *rect);

/* interface_undo.c */
struct uiUndoStack_Text *ui_textedit_undo_stack_create(void);
void ui_textedit_undo_stack_destroy(struct uiUndoStack_Text *undo_stack);
void ui_textedit_undo_push(struct uiUndoStack_Text *undo_stack,
                           const char *text,
                           int cursor_index);
const char *ui_textedit_undo(struct uiUndoStack_Text *undo_stack,
                             int direction,
                             int *r_cursor_index);

/* interface_handlers.c */
PointerRNA *ui_handle_afterfunc_add_operator(struct wmOperatorType *ot,
                                             int opcontext,
                                             bool create_props);
extern void ui_pan_to_scroll(const struct wmEvent *event, int *type, int *val);
extern void ui_but_activate_event(struct bContext *C, struct ARegion *region, uiBut *but);
extern void ui_but_activate_over(struct bContext *C, struct ARegion *region, uiBut *but);
extern void ui_but_execute_begin(struct bContext *C,
                                 struct ARegion *region,
                                 uiBut *but,
                                 void **active_back);
extern void ui_but_execute_end(struct bContext *C,
                               struct ARegion *region,
                               uiBut *but,
                               void *active_back);
extern void ui_but_active_free(const struct bContext *C, uiBut *but);
extern int ui_but_menu_direction(uiBut *but);
extern void ui_but_text_password_hide(char password_str[UI_MAX_DRAW_STR],
                                      uiBut *but,
                                      const bool restore);
extern uiBut *ui_but_find_select_in_enum(uiBut *but, int direction);
bool ui_but_is_editing(const uiBut *but);
float ui_block_calc_pie_segment(struct uiBlock *block, const float event_xy[2]);

void ui_but_add_shortcut(uiBut *but, const char *shortcut_str, const bool do_strip);
void ui_but_clipboard_free(void);
bool ui_but_rna_equals(const uiBut *a, const uiBut *b);
bool ui_but_rna_equals_ex(const uiBut *but,
                          const PointerRNA *ptr,
                          const PropertyRNA *prop,
                          int index);
uiBut *ui_but_find_old(uiBlock *block_old, const uiBut *but_new);
uiBut *ui_but_find_new(uiBlock *block_new, const uiBut *but_old);

#ifdef WITH_INPUT_IME
void ui_but_ime_reposition(uiBut *but, int x, int y, bool complete);
struct wmIMEData *ui_but_ime_data_get(uiBut *but);
#endif

/* interface_widgets.c */

/* Widget shader parameters, must match the shader layout. */
typedef struct uiWidgetBaseParameters {
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
} uiWidgetBaseParameters;

enum {
  ROUNDBOX_TRIA_NONE = 0,
  ROUNDBOX_TRIA_ARROWS,
  ROUNDBOX_TRIA_SCROLL,
  ROUNDBOX_TRIA_MENU,
  ROUNDBOX_TRIA_CHECK,
  ROUNDBOX_TRIA_HOLD_ACTION_ARROW,

  ROUNDBOX_TRIA_MAX, /* don't use */
};

struct GPUBatch *ui_batch_roundbox_widget_get(void);
struct GPUBatch *ui_batch_roundbox_shadow_get(void);

void ui_draw_anti_tria_rect(const rctf *rect, char dir, const float color[4]);
void ui_draw_menu_back(struct uiStyle *style, uiBlock *block, rcti *rect);
void ui_draw_box_opaque(rcti *rect, int roundboxalign);
void ui_draw_popover_back(struct ARegion *region,
                          struct uiStyle *style,
                          uiBlock *block,
                          rcti *rect);
void ui_draw_pie_center(uiBlock *block);
const struct uiWidgetColors *ui_tooltip_get_theme(void);

void ui_draw_widget_menu_back_color(const rcti *rect, bool use_shadow, const float color[4]);
void ui_draw_widget_menu_back(const rcti *rect, bool use_shadow);
void ui_draw_tooltip_background(const struct uiStyle *style, uiBlock *block, rcti *rect);

extern void ui_draw_but(const struct bContext *C,
                        struct ARegion *region,
                        struct uiStyle *style,
                        uiBut *but,
                        rcti *rect);

/**
 * Info about what the separator character separates, used to decide between different drawing
 * styles. E.g. we never want a shortcut string to be clipped, but other hint strings can be
 * clipped.
 */
typedef enum {
  UI_MENU_ITEM_SEPARATOR_NONE,
  /** Separator is used to indicate shortcut string of this item. Shortcut string will not get
   * clipped. */
  UI_MENU_ITEM_SEPARATOR_SHORTCUT,
  /** Separator is used to indicate some additional hint to display for this item. Hint string will
   * get clipped before the normal text. */
  UI_MENU_ITEM_SEPARATOR_HINT,
} uiMenuItemSeparatorType;
void ui_draw_menu_item(const struct uiFontStyle *fstyle,
                       rcti *rect,
                       const char *name,
                       int iconid,
                       int state,
                       uiMenuItemSeparatorType separator_type,
                       int *r_xmax);
void ui_draw_preview_item(
    const struct uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state);

#define UI_TEXT_MARGIN_X 0.4f
#define UI_POPUP_MARGIN (UI_DPI_FAC * 12)
/* Margin at top of screen for popups. Note this value must be sufficient
   to draw a popover arrow to avoid cropping it. */
#define UI_POPUP_MENU_TOP (int)(10 * UI_DPI_FAC)

#define UI_PIXEL_AA_JITTER 8
extern const float ui_pixel_jitter[UI_PIXEL_AA_JITTER][2];

/* interface_style.c */
void uiStyleInit(void);

/* interface_icons.c */
void ui_icon_ensure_deferred(const struct bContext *C, const int icon_id, const bool big);
int ui_id_icon_get(const struct bContext *C, struct ID *id, const bool big);

/* interface_icons_event.c */
void icon_draw_rect_input(
    float x, float y, int w, int h, float alpha, short event_type, short event_value);

/* resources.c */
void ui_resources_init(void);
void ui_resources_free(void);

/* interface_layout.c */
void ui_layout_add_but(uiLayout *layout, uiBut *but);
bool ui_layout_replace_but_ptr(uiLayout *layout, const void *old_but_ptr, uiBut *new_but);
uiBut *ui_but_add_search(uiBut *but,
                         PointerRNA *ptr,
                         PropertyRNA *prop,
                         PointerRNA *searchptr,
                         PropertyRNA *searchprop);
void ui_layout_list_set_labels_active(uiLayout *layout);
/* menu callback */
void ui_item_menutype_func(struct bContext *C, struct uiLayout *layout, void *arg_mt);
void ui_item_paneltype_func(struct bContext *C, struct uiLayout *layout, void *arg_pt);

/* interface_button_group.c */
void ui_block_new_button_group(uiBlock *block, uiButtonGroupFlag flag);
void ui_button_group_add_but(uiBlock *block, uiBut *but);
void ui_button_group_replace_but_ptr(uiBlock *block, const void *old_but_ptr, uiBut *new_but);
void ui_block_free_button_groups(uiBlock *block);

/* interface_align.c */
bool ui_but_can_align(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
int ui_but_align_opposite_to_area_align_get(const struct ARegion *region) ATTR_WARN_UNUSED_RESULT;
void ui_block_align_calc(uiBlock *block, const struct ARegion *region);

/* interface_anim.c */
void ui_but_anim_flag(uiBut *but, const struct AnimationEvalContext *anim_eval_context);
void ui_but_anim_copy_driver(struct bContext *C);
void ui_but_anim_paste_driver(struct bContext *C);
bool ui_but_anim_expression_get(uiBut *but, char *str, size_t maxlen);
bool ui_but_anim_expression_set(uiBut *but, const char *str);
bool ui_but_anim_expression_create(uiBut *but, const char *str);
void ui_but_anim_autokey(struct bContext *C, uiBut *but, struct Scene *scene, float cfra);

void ui_but_anim_decorate_cb(struct bContext *C, void *arg_but, void *arg_dummy);
void ui_but_anim_decorate_update_from_flag(uiButDecorator *but);

/* interface_query.c */
bool ui_but_is_editable(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_editable_as_text(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_toggle(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_interactive(const uiBut *but, const bool labeledit) ATTR_WARN_UNUSED_RESULT;
bool ui_but_is_popover_once_compat(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
bool ui_but_has_array_value(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
int ui_but_icon(const uiBut *but);
void ui_but_pie_dir(RadialDirection dir, float vec[2]);

bool ui_but_is_cursor_warp(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

bool ui_but_contains_pt(const uiBut *but, float mx, float my) ATTR_WARN_UNUSED_RESULT;
bool ui_but_contains_rect(const uiBut *but, const rctf *rect);
bool ui_but_contains_point_px_icon(const uiBut *but,
                                   struct ARegion *region,
                                   const struct wmEvent *event) ATTR_WARN_UNUSED_RESULT;
bool ui_but_contains_point_px(const uiBut *but, const struct ARegion *region, int x, int y)
    ATTR_WARN_UNUSED_RESULT;

uiBut *ui_list_find_mouse_over(struct ARegion *region,
                               const struct wmEvent *event) ATTR_WARN_UNUSED_RESULT;

uiBut *ui_but_find_mouse_over_ex(struct ARegion *region,
                                 const int x,
                                 const int y,
                                 const bool labeledit) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_find_mouse_over(struct ARegion *region,
                              const struct wmEvent *event) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_find_rect_over(const struct ARegion *region,
                             const rcti *rect_px) ATTR_WARN_UNUSED_RESULT;

uiBut *ui_list_find_mouse_over_ex(struct ARegion *region, int x, int y) ATTR_WARN_UNUSED_RESULT;

bool ui_but_contains_password(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

size_t ui_but_drawstr_len_without_sep_char(const uiBut *but);
size_t ui_but_tip_len_only_first_line(const uiBut *but);

uiBut *ui_but_prev(uiBut *but) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_next(uiBut *but) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_first(uiBlock *block) ATTR_WARN_UNUSED_RESULT;
uiBut *ui_but_last(uiBlock *block) ATTR_WARN_UNUSED_RESULT;

bool ui_block_is_menu(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
bool ui_block_is_popover(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
bool ui_block_is_pie_menu(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
bool ui_block_is_popup_any(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;

uiBlock *ui_block_find_mouse_over_ex(const struct ARegion *region,
                                     const int x,
                                     const int y,
                                     bool only_clip);
uiBlock *ui_block_find_mouse_over(const struct ARegion *region,
                                  const struct wmEvent *event,
                                  bool only_clip);

uiBut *ui_region_find_first_but_test_flag(struct ARegion *region,
                                          int flag_include,
                                          int flag_exclude);
uiBut *ui_region_find_active_but(struct ARegion *region) ATTR_WARN_UNUSED_RESULT;
bool ui_region_contains_point_px(const struct ARegion *region,
                                 int x,
                                 int y) ATTR_WARN_UNUSED_RESULT;
bool ui_region_contains_rect_px(const struct ARegion *region, const rcti *rect_px);

struct ARegion *ui_screen_region_find_mouse_over_ex(struct bScreen *screen, int x, int y);
struct ARegion *ui_screen_region_find_mouse_over(struct bScreen *screen,
                                                 const struct wmEvent *event);

/* interface_context_menu.c */
bool ui_popup_context_menu_for_button(struct bContext *C, uiBut *but);
void ui_popup_context_menu_for_panel(struct bContext *C,
                                     struct ARegion *region,
                                     struct Panel *panel);

/* interface_eyedropper.c */
struct wmKeyMap *eyedropper_modal_keymap(struct wmKeyConfig *keyconf);
struct wmKeyMap *eyedropper_colorband_modal_keymap(struct wmKeyConfig *keyconf);

/* interface_eyedropper_color.c */
void UI_OT_eyedropper_color(struct wmOperatorType *ot);

/* interface_eyedropper_colorband.c */
void UI_OT_eyedropper_colorramp(struct wmOperatorType *ot);
void UI_OT_eyedropper_colorramp_point(struct wmOperatorType *ot);

/* interface_eyedropper_datablock.c */
void UI_OT_eyedropper_id(struct wmOperatorType *ot);

/* interface_eyedropper_depth.c */
void UI_OT_eyedropper_depth(struct wmOperatorType *ot);

/* interface_eyedropper_driver.c */
void UI_OT_eyedropper_driver(struct wmOperatorType *ot);

/* interface_eyedropper_gpencil_color.c */
void UI_OT_eyedropper_gpencil_color(struct wmOperatorType *ot);

/**
 * For use with #ui_rna_collection_search_update_fn.
 */
typedef struct uiRNACollectionSearch {
  PointerRNA target_ptr;
  PropertyRNA *target_prop;

  PointerRNA search_ptr;
  PropertyRNA *search_prop;

  uiBut *search_but;
  /* Let UI_butstore_ API update search_but pointer above over redraws. */
  uiButStore *butstore;
  /* Block has to be stored for freeing butstore (uiBut.block doesn't work with undo). */
  uiBlock *butstore_block;
} uiRNACollectionSearch;
void ui_rna_collection_search_update_fn(const struct bContext *C,
                                        void *arg,
                                        const char *str,
                                        uiSearchItems *items);

/* interface_ops.c */
bool ui_jump_to_target_button_poll(struct bContext *C);

/* interface_queries.c */
void ui_interface_tag_script_reload_queries(void);
