/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <variant>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_array.hh"
#include "BLI_array_utils.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_rect.h"
#include "BLI_sort_utils.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_blender_undo.hh"
#include "BKE_brush.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curveprofile.h"
#include "BKE_movieclip.h"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_tracking.h"
#include "BKE_unit.hh"

#include "BLT_translation.hh"

#include "GHOST_C-api.h"

#include "IMB_colormanagement.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "UI_abstract_view.hh"
#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_string_search.hh"

#include "BLF_api.hh"

#include "interface_intern.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "CLG_log.h"

#include "WM_api.hh"
#include "WM_types.hh"
#include "wm_event_system.hh"

#ifdef WITH_INPUT_IME
#  include "wm_window.hh"
#endif

using blender::StringRef;

static CLG_LogRef LOG = {"ui.handler"};

/* -------------------------------------------------------------------- */
/** \name Feature Defines
 *
 * These defines allow developers to locally toggle functionality which
 * may be useful for testing (especially conflicts in dragging).
 * Ideally the code would be refactored to support this functionality in a less fragile way.
 * Until then keep these defines.
 * \{ */

/** Place the mouse at the scaled down location when un-grabbing. */
#define USE_CONT_MOUSE_CORRECT
/** Support dragging toggle buttons. */
#define USE_DRAG_TOGGLE

/** Support dragging multiple number buttons at once. */
#define USE_DRAG_MULTINUM

/** Allow dragging/editing all other selected items at once. */
#define USE_ALLSELECT

/**
 * Check to avoid very small mouse-moves from jumping away from keyboard navigation,
 * while larger mouse motion will override keyboard input, see: #34936.
 */
#define USE_KEYNAV_LIMIT

/** Support dragging popups by their header. */
#define USE_DRAG_POPUP

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Defines
 * \{ */

/**
 * The buffer side used for password strings, where the password is stored internally,
 * but not displayed.
 */
#define UI_MAX_PASSWORD_STR 128

/**
 * This is a lower limit on the soft minimum of the range.
 * Usually the derived lower limit from the visible precision is higher,
 * so this number is the backup minimum.
 *
 * Logarithmic scale does not work with a minimum value of zero,
 * but we want to support it anyway. It is set to 0.5e... for
 * correct rounding since when the tweaked value is lower than
 * the log minimum (lower limit), it will snap to 0.
 */
#define UI_PROP_SCALE_LOG_MIN 0.5e-8f
/**
 * This constant defines an offset for the precision change in
 * snap rounding, when going to higher values. It is set to
 * `0.5 - log10(3) = 0.03` to make the switch at `0.3` values.
 */
#define UI_PROP_SCALE_LOG_SNAP_OFFSET 0.03f

/**
 * When #USER_CONTINUOUS_MOUSE is disabled or tablet input is used,
 * Use this as a maximum soft range for mapping cursor motion to the value.
 * Otherwise min/max of #FLT_MAX, #INT_MAX cause small adjustments to jump to large numbers.
 *
 * This is needed for values such as location & dimensions which don't have a meaningful min/max,
 * Instead of mapping cursor motion to the min/max, map the motion to the click-step.
 *
 * This value is multiplied by the click step to calculate a range to clamp the soft-range by.
 * See: #68130
 */
#define UI_DRAG_MAP_SOFT_RANGE_PIXEL_MAX 1000

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Prototypes
 * \{ */

struct uiBlockInteraction_Handle;
struct uiTextEdit;

static int ui_do_but_EXIT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event);
static bool ui_but_find_select_in_enum__cmp(const uiBut *but_a, const uiBut *but_b);
static void ui_textedit_string_set(uiBut *but, uiTextEdit &text_edit, const char *str);
static void button_tooltip_timer_reset(bContext *C, uiBut *but);

static void ui_block_interaction_begin_ensure(bContext *C,
                                              uiBlock *block,
                                              uiHandleButtonData *data,
                                              const bool is_click);
static uiBlockInteraction_Handle *ui_block_interaction_begin(bContext *C,
                                                             uiBlock *block,
                                                             const bool is_click);
static void ui_block_interaction_end(bContext *C,
                                     uiBlockInteraction_CallbackData *callbacks,
                                     uiBlockInteraction_Handle *interaction);
static void ui_block_interaction_update(bContext *C,
                                        uiBlockInteraction_CallbackData *callbacks,
                                        uiBlockInteraction_Handle *interaction);

#ifdef USE_KEYNAV_LIMIT
static void ui_mouse_motion_keynav_init(uiKeyNavLock *keynav, const wmEvent *event);
static bool ui_mouse_motion_keynav_test(uiKeyNavLock *keynav, const wmEvent *event);
#endif

static void with_but_active_as_semi_modal(bContext *C,
                                          ARegion *region,
                                          uiBut *but,
                                          blender::FunctionRef<void()> fn);
static int ui_handle_region_semi_modal_buttons(bContext *C, const wmEvent *event, ARegion *region);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Structs & Defines
 * \{ */

#define BUTTON_FLASH_DELAY 0.020
#define MENU_SCROLL_INTERVAL 0.1
#define PIE_MENU_INTERVAL 0.01
#define BUTTON_AUTO_OPEN_THRESH 0.2
#define BUTTON_MOUSE_TOWARDS_THRESH 1.0
/** Pixels to move the cursor to get out of keyboard navigation. */
#define BUTTON_KEYNAV_PX_LIMIT 8

/** Margin around the menu, use to check if we're moving towards this rectangle (in pixels). */
#define MENU_TOWARDS_MARGIN 20
/** Tolerance for closing menus (in pixels). */
#define MENU_TOWARDS_WIGGLE_ROOM 64

enum uiButtonActivateType {
  BUTTON_ACTIVATE_OVER,
  BUTTON_ACTIVATE,
  BUTTON_ACTIVATE_APPLY,
  BUTTON_ACTIVATE_TEXT_EDITING,
  BUTTON_ACTIVATE_OPEN,
};

enum uiHandleButtonState {
  BUTTON_STATE_INIT,
  BUTTON_STATE_HIGHLIGHT,
  BUTTON_STATE_WAIT_FLASH,
  BUTTON_STATE_WAIT_RELEASE,
  BUTTON_STATE_WAIT_KEY_EVENT,
  BUTTON_STATE_NUM_EDITING,
  BUTTON_STATE_TEXT_EDITING,
  BUTTON_STATE_TEXT_SELECTING,
  BUTTON_STATE_MENU_OPEN,
  BUTTON_STATE_WAIT_DRAG,
  BUTTON_STATE_EXIT,
};

enum uiMenuScrollType {
  MENU_SCROLL_UP,
  MENU_SCROLL_DOWN,
  MENU_SCROLL_TOP,
  MENU_SCROLL_BOTTOM,
};

struct uiBlockInteraction_Handle {
  uiBlockInteraction_Params params;
  void *user_data;
  /**
   * This is shared between #uiHandleButtonData and #uiAfterFunc,
   * the last user runs the end callback and frees the data.
   *
   * This is needed as the order of freeing changes depending on
   * accepting/canceling the operation.
   */
  int user_count;
};

#ifdef USE_ALLSELECT

/* Unfortunately there's no good way handle more generally:
 * (propagate single clicks on layer buttons to other objects) */
#  define USE_ALLSELECT_LAYER_HACK

struct uiSelectContextElem {
  PointerRNA ptr;
  union {
    bool val_b;
    int val_i;
    float val_f;
  };
};

struct uiSelectContextStore {
  blender::Vector<uiSelectContextElem> elems;
  bool do_free = false;
  bool is_enabled = false;
  /* When set, simply copy values (don't apply difference).
   * Rules are:
   * - dragging numbers uses delta.
   * - typing in values will assign to all. */
  bool is_copy = false;
};

static bool ui_selectcontext_begin(bContext *C, uiBut *but, uiSelectContextStore *selctx_data);
static void ui_selectcontext_end(uiBut *but, uiSelectContextStore *selctx_data);
static void ui_selectcontext_apply(bContext *C,
                                   uiBut *but,
                                   uiSelectContextStore *selctx_data,
                                   const double value,
                                   const double value_orig);

/**
 * Ideally we would only respond to events which are expected to be used for multi button editing
 * (additionally checking if this is a mouse[wheel] or return-key event to avoid the ALT conflict
 * with button array pasting, see #108096, but unfortunately wheel events are not part of
 * `win->eventstate` with modifiers held down. Instead, the conflict is avoided by specifically
 * filtering out CTRL ALT V in #ui_apply_but(). */
#  define IS_ALLSELECT_EVENT(event) (((event)->modifier & KM_ALT) != 0)

/** just show a tinted color so users know its activated */
#  define UI_BUT_IS_SELECT_CONTEXT UI_BUT_NODE_ACTIVE

#endif /* USE_ALLSELECT */

#ifdef USE_DRAG_MULTINUM

/**
 * how far to drag before we check for gesture direction (in pixels),
 * NOTE: half the height of a button is about right... */
#  define DRAG_MULTINUM_THRESHOLD_DRAG_X (UI_UNIT_Y / 4)

/**
 * How far to drag horizontally
 * before we stop checking which buttons the gesture spans (in pixels),
 * locking down the buttons so we can drag freely without worrying about vertical movement.
 */
#  define DRAG_MULTINUM_THRESHOLD_DRAG_Y (UI_UNIT_Y / 4)

/**
 * How strict to be when detecting a vertical gesture:
 * [0.5 == sloppy], [0.9 == strict], (unsigned dot-product).
 *
 * \note We should be quite strict here,
 * since doing a vertical gesture by accident should be avoided,
 * however with some care a user should be able to do a vertical movement without _missing_.
 */
#  define DRAG_MULTINUM_THRESHOLD_VERTICAL (0.75f)

/* a simple version of uiHandleButtonData when accessing multiple buttons */
struct uiButMultiState {
  double origvalue = 0.0f;
  uiBut *but = nullptr;

#  ifdef USE_ALLSELECT
  uiSelectContextStore select_others;
#  endif
};

struct uiHandleButtonMulti {
  enum {
    /** gesture direction unknown, wait until mouse has moved enough... */
    INIT_UNSET = 0,
    /** vertical gesture detected, flag buttons interactively (UI_BUT_DRAG_MULTI) */
    INIT_SETUP,
    /** flag buttons finished, apply horizontal motion to active and flagged */
    INIT_ENABLE,
    /** vertical gesture _not_ detected, take no further action */
    INIT_DISABLE,
  } init;

  bool has_mbuts = false; /* any buttons flagged UI_BUT_DRAG_MULTI */
  LinkNode *mbuts = nullptr;
  uiButStore *bs_mbuts = nullptr;

  bool is_proportional = false;

  /* In some cases we directly apply the changes to multiple buttons,
   * so we don't want to do it twice. */
  bool skip = false;

  /* before activating, we need to check gesture direction accumulate signed cursor movement
   * here so we can tell if this is a vertical motion or not. */
  float drag_dir[2] = {0.0f, 0.0f};

  /* values copied direct from event->xy
   * used to detect buttons between the current and initial mouse position */
  int drag_start[2] = {0, 0};

  /* store x location once INIT_SETUP is set,
   * moving outside this sets INIT_ENABLE */
  int drag_lock_x = 0;
};

#endif /* USE_DRAG_MULTINUM */

/**
 * Data for editing the value of the button as text.
 */
struct uiTextEdit {
  /** The currently displayed/edited string, use 'ui_textedit_string_set' to assign new strings. */
  char *edit_string = nullptr;
  /* Maximum string size the button accepts, and as such the maximum size for #edit_string
   * (including terminator). */
  int max_string_size = 0;
  /* Allow reallocating #edit_string and using #max_string_size to track alloc size (maxlen + 1) */
  bool is_str_dynamic = false;
  char *original_string = nullptr;

  /* Button text selection:
   * extension direction, selextend, inside ui_do_but_TEX */
  int sel_pos_init = 0;

  /* Text field undo. */
  uiUndoStack_Text *undo_stack_text = nullptr;
};

struct uiHandleButtonData {
  wmWindowManager *wm = nullptr;
  wmWindow *window = nullptr;
  ScrArea *area = nullptr;
  ARegion *region = nullptr;

  bool interactive = false;

  /* overall state */
  uiHandleButtonState state = {};
  int retval = 0;
  /* booleans (could be made into flags) */
  bool cancel = false;
  bool escapecancel = false;
  bool applied = false;
  bool applied_interactive = false;
  /* Button is being applied through an extra icon. */
  bool apply_through_extra_icon = false;
  bool changed_cursor = false;
  wmTimer *flashtimer = nullptr;

  uiTextEdit text_edit;

  double value = 0.0f;
  double origvalue = 0.0f;
  double startvalue = 0.0f;
  float vec[3], origvec[3];
  ColorBand *coba = nullptr;

  /* True when alt is held and the preference for displaying tooltips should be ignored. */
  bool tooltip_force = false;
  /**
   * Behave as if #UI_BUT_DISABLED is set (without drawing grayed out).
   * Needed so non-interactive labels can be activated for the purpose of showing tool-tips,
   * without them blocking interaction with nodes, see: #97386.
   */
  bool disable_force = false;

  /**
   * Semi-modal buttons: Instead of capturing all events, pass on events that aren't relevant to
   * own handling. This way a text button (e.g. a search/filter field) can stay active while the
   * remaining UI stays interactive. Only few button types support this well currently.
   */
  bool is_semi_modal = false;

  /* auto open */
  bool used_mouse = false;
  wmTimer *autoopentimer = nullptr;

  /* auto open (hold) */
  wmTimer *hold_action_timer = nullptr;

  /* number editing / dragging */
  /* coords are Window/uiBlock relative (depends on the button) */
  int draglastx = 0;
  int draglasty = 0;
  int dragstartx = 0;
  int dragstarty = 0;
  bool dragchange = false;
  bool draglock = false;
  int dragsel = 0;
  float dragf = 0.0f;
  float dragfstart = 0.0f;
  CBData *dragcbd = nullptr;

  /** Soft min/max with #UI_DRAG_MAP_SOFT_RANGE_PIXEL_MAX applied. */
  float drag_map_soft_min = 0.0f;
  float drag_map_soft_max = 0.0f;

#ifdef USE_CONT_MOUSE_CORRECT
  /* when ungrabbing buttons which are #ui_but_is_cursor_warp(),
   * we may want to position them.
   * FLT_MAX signifies do-nothing, use #ui_block_to_window_fl()
   * to get this into a usable space. */
  float ungrab_mval[2];
#endif

  /* Menu open, see: #UI_screen_free_active_but_highlight. */
  uiPopupBlockHandle *menu = nullptr;

  /* Search box see: #UI_screen_free_active_but_highlight. */
  ARegion *searchbox = nullptr;
#ifdef USE_KEYNAV_LIMIT
  uiKeyNavLock searchbox_keynav_state;
#endif

#ifdef USE_DRAG_MULTINUM
  /* Multi-buttons will be updated in unison with the active button. */
  uiHandleButtonMulti multi_data;
#endif

#ifdef USE_ALLSELECT
  uiSelectContextStore select_others;
#endif

  uiBlockInteraction_Handle *custom_interaction_handle = nullptr;

  /* post activate */
  uiButtonActivateType posttype = {};
  uiBut *postbut = nullptr;
};

struct uiAfterFunc {
  uiAfterFunc *next, *prev;

  uiButHandleFunc func;
  void *func_arg1;
  void *func_arg2;
  /** C++ version of #func above, without need for void pointer arguments. */
  std::function<void(bContext &)> apply_func;

  uiButHandleNFunc funcN;
  void *func_argN;
  uiButArgNFree func_argN_free_fn;
  /* uiButArgNCopy func_argN_copy_fn is not needed currently. */

  uiButHandleRenameFunc rename_func;
  void *rename_arg1;
  void *rename_orig;

  std::function<void(std::string &new_name)> rename_full_func = nullptr;
  std::string rename_full_new;

  uiBlockHandleFunc handle_func;
  void *handle_func_arg;
  int retval;

  wmOperator *popup_op;
  wmOperatorType *optype;
  blender::wm::OpCallContext opcontext;
  PointerRNA *opptr;

  PointerRNA rnapoin;
  PropertyRNA *rnaprop;

  void *search_arg;
  uiFreeArgFunc search_arg_free_fn;

  uiBlockInteraction_CallbackData custom_interaction_callbacks;
  uiBlockInteraction_Handle *custom_interaction_handle;

  std::optional<bContextStore> context;

  char undostr[BKE_UNDO_STR_MAX];
  std::string drawstr;
};

static void button_activate_init(bContext *C,
                                 ARegion *region,
                                 uiBut *but,
                                 uiButtonActivateType type);
static void button_activate_state(bContext *C, uiBut *but, uiHandleButtonState state);
static void button_activate_exit(
    bContext *C, uiBut *but, uiHandleButtonData *data, const bool mousemove, const bool onfree);
static int ui_handler_region_menu(bContext *C, const wmEvent *event, void *userdata);
static void ui_handle_button_activate(bContext *C,
                                      ARegion *region,
                                      uiBut *but,
                                      uiButtonActivateType type);
static bool ui_do_but_extra_operator_icon(bContext *C,
                                          uiBut *but,
                                          uiHandleButtonData *data,
                                          const wmEvent *event);
static void ui_do_but_extra_operator_icons_mousemove(uiBut *but,
                                                     uiHandleButtonData *data,
                                                     const wmEvent *event);
static void ui_numedit_begin_set_values(uiBut *but, uiHandleButtonData *data);

#ifdef USE_DRAG_MULTINUM
static void ui_multibut_restore(bContext *C, uiHandleButtonData *data, uiBlock *block);
static uiButMultiState *ui_multibut_lookup(uiHandleButtonData *data, const uiBut *but);
#endif

/* buttons clipboard */
static ColorBand but_copypaste_coba = {};
static CurveMapping but_copypaste_curve = {};
static bool but_copypaste_curve_alive = false;
static CurveProfile but_copypaste_profile = {};
static bool but_copypaste_profile_alive = false;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Struct allocation & freeing
 * \{ */

void ui_but_handle_data_free(uiHandleButtonData **data)
{
  MEM_delete(*data);
  *data = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Queries
 * \{ */

bool ui_but_is_editing(const uiBut *but)
{
  const uiHandleButtonData *data = but->active;
  return (data && ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING));
}

void ui_pan_to_scroll(const wmEvent *event, int *type, int *val)
{
  static int lastdy = 0;
  const int dy = WM_event_absolute_delta_y(event);

  /* This event should be originally from event->type,
   * converting wrong event into wheel is bad, see #33803. */
  BLI_assert(*type == MOUSEPAN);

  /* sign differs, reset */
  if ((dy > 0 && lastdy < 0) || (dy < 0 && lastdy > 0)) {
    lastdy = dy;
  }
  else {
    lastdy += dy;

    if (abs(lastdy) > int(UI_UNIT_Y)) {
      *val = KM_PRESS;

      if (dy > 0) {
        *type = WHEELUPMOUSE;
      }
      else {
        *type = WHEELDOWNMOUSE;
      }

      lastdy = 0;
    }
  }
}

static bool ui_but_find_select_in_enum__cmp(const uiBut *but_a, const uiBut *but_b)
{
  return ((but_a->type == but_b->type) && (but_a->alignnr == but_b->alignnr) &&
          (but_a->poin == but_b->poin) && (but_a->rnapoin.type == but_b->rnapoin.type) &&
          (but_a->rnaprop == but_b->rnaprop));
}

uiBut *ui_but_find_select_in_enum(uiBut *but, int direction)
{
  uiBlock *block = but->block;
  int i = block->but_index(but);
  uiBut *but_found = nullptr;
  BLI_assert(ELEM(direction, -1, 1));

  while (i > 0 && ui_but_find_select_in_enum__cmp(block->buttons[i - 1].get(), but)) {
    i--;
  }

  while (i < block->buttons.size() &&
         ui_but_find_select_in_enum__cmp(block->buttons[i].get(), but))
  {
    if (block->buttons[i]->flag & UI_SELECT) {
      but_found = block->buttons[i].get();
      if (direction == 1) {
        break;
      }
    }
    i++;
  }

  return but_found;
}

static float ui_mouse_scale_warp_factor(const bool shift)
{
  return shift ? 0.05f : 1.0f;
}

static void ui_mouse_scale_warp(uiHandleButtonData *data,
                                const float mx,
                                const float my,
                                float *r_mx,
                                float *r_my,
                                const bool shift)
{
  const float fac = ui_mouse_scale_warp_factor(shift);

  /* slow down the mouse, this is fairly picky */
  *r_mx = (data->dragstartx * (1.0f - fac) + mx * fac);
  *r_my = (data->dragstarty * (1.0f - fac) + my * fac);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Utilities
 * \{ */

#ifdef USE_DRAG_MULTINUM
static bool ui_multibut_drag_wait(const uiHandleButtonMulti &multi_data)
{
  const bool initializing = ELEM(
      multi_data.init, uiHandleButtonMulti::INIT_UNSET, uiHandleButtonMulti::INIT_SETUP);
  const bool vertical_dragging = abs(multi_data.drag_dir[1]) > abs(multi_data.drag_dir[0]);

  /* Continue waiting if we are dragging vertically but have not yet detected gesture. */
  return (initializing && vertical_dragging);
}
#endif /* USE_DRAG_MULTINUM */

/**
 * Ignore mouse movements within some horizontal pixel threshold before starting to drag
 */
static bool ui_but_dragedit_update_mval(uiHandleButtonData *data,
                                        int mx,
                                        blender::FunctionRef<int()> drag_threshold_fn)
{
  if (data->draglock) {
    const int threshold = drag_threshold_fn();
    if (abs(mx - data->dragstartx) < threshold) {
      return false;
    }
#ifdef USE_DRAG_MULTINUM
    /* Continue to wait for multibut drag initialization if dragging vertically. */
    if (ui_multibut_drag_wait(data->multi_data)) {
      return false;
    }
#endif
    data->draglock = false;
    data->dragstartx = mx; /* ignore mouse movement within drag-lock */
  }

  return true;
}

static bool ui_rna_is_userdef(PointerRNA *ptr, PropertyRNA *prop)
{
  /* Not very elegant, but ensures preference changes force re-save. */

  if (!prop) {
    return false;
  }
  if (RNA_property_flag(prop) & PROP_NO_DEG_UPDATE) {
    return false;
  }

  StructRNA *base = RNA_struct_base(ptr->type);
  if (base == nullptr) {
    base = ptr->type;
  }

  bool is_userdef = false;
  if (ELEM(base,
           &RNA_AddonPreferences,
           &RNA_KeyConfigPreferences,
           &RNA_KeyMapItem,
           &RNA_UserAssetLibrary))
  {
    is_userdef = true;
  }
  else if (ptr->owner_id) {
    switch (GS(ptr->owner_id->name)) {
      case ID_WM: {
        for (const AncestorPointerRNA &ancestor : ptr->ancestors) {
          if (RNA_struct_is_a(ancestor.type, &RNA_KeyConfigPreferences)) {
            is_userdef = true;
            break;
          }
        }
        break;
      }
      default: {
        break;
      }
    }
  }
  else if (ptr->owner_id == nullptr) {
    for (const AncestorPointerRNA &ancestor : ptr->ancestors) {
      if (RNA_struct_is_a(ancestor.type, &RNA_AddonPreferences)) {
        is_userdef = true;
        break;
      }
    }
  }

  return is_userdef;
}

bool UI_but_is_userdef(const uiBut *but)
{
  /* This is read-only, RNA API isn't using const when it could. */
  return ui_rna_is_userdef((PointerRNA *)&but->rnapoin, but->rnaprop);
}

static void ui_rna_update_preferences_dirty(PointerRNA *ptr, PropertyRNA *prop)
{
  if (ui_rna_is_userdef(ptr, prop)) {
    U.runtime.is_dirty = true;
    WM_main_add_notifier(NC_WINDOW, nullptr);
  }
}

static void ui_but_update_preferences_dirty(uiBut *but)
{
  ui_rna_update_preferences_dirty(&but->rnapoin, but->rnaprop);
}

static void ui_afterfunc_update_preferences_dirty(uiAfterFunc *after)
{
  ui_rna_update_preferences_dirty(&after->rnapoin, after->rnaprop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Snap Values
 * \{ */

enum eSnapType {
  SNAP_OFF = 0,
  SNAP_ON,
  SNAP_ON_SMALL,
};

static enum eSnapType ui_event_to_snap(const wmEvent *event)
{
  return (event->modifier & KM_CTRL) ? (event->modifier & KM_SHIFT) ? SNAP_ON_SMALL : SNAP_ON :
                                       SNAP_OFF;
}

static bool ui_event_is_snap(const wmEvent *event)
{
  return (ELEM(event->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY) ||
          ELEM(event->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY));
}

static void ui_color_snap_hue(const enum eSnapType snap, float *r_hue)
{
  const float snap_increment = (snap == SNAP_ON_SMALL) ? 24 : 12;
  BLI_assert(snap != SNAP_OFF);
  *r_hue = roundf((*r_hue) * snap_increment) / snap_increment;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Apply/Revert
 * \{ */

static ListBase UIAfterFuncs = {nullptr, nullptr};

static uiAfterFunc *ui_afterfunc_new()
{
  uiAfterFunc *after = MEM_new<uiAfterFunc>(__func__);
  /* Safety asserts to check if members were 0 initialized properly. */
  BLI_assert(after->next == nullptr && after->prev == nullptr);
  BLI_assert(after->undostr[0] == '\0');

  BLI_addtail(&UIAfterFuncs, after);

  return after;
}

/**
 * For executing operators after the button is pressed.
 * (some non operator buttons need to trigger operators), see: #37795.
 *
 * \param context_but: A button from which to get the context from (`uiBut.context`) for the
 *                     operator execution.
 *
 * \note Ownership over \a properties is moved here. The #uiAfterFunc owns it now.
 * \note Can only call while handling buttons.
 */
static void ui_handle_afterfunc_add_operator_ex(wmOperatorType *ot,
                                                PointerRNA **properties,
                                                blender::wm::OpCallContext opcontext,
                                                const uiBut *context_but)
{
  uiAfterFunc *after = ui_afterfunc_new();

  after->optype = ot;
  after->opcontext = opcontext;
  if (properties) {
    after->opptr = *properties;
    *properties = nullptr;
  }

  if (context_but && context_but->context) {
    after->context = *context_but->context;
  }

  if (context_but) {
    after->drawstr = ui_but_drawstr_without_sep_char(context_but);
  }
}

void ui_handle_afterfunc_add_operator(wmOperatorType *ot, blender::wm::OpCallContext opcontext)
{
  ui_handle_afterfunc_add_operator_ex(ot, nullptr, opcontext, nullptr);
}

static void popup_check(bContext *C, wmOperator *op)
{
  if (op && op->type->check) {
    op->type->check(C, op);
  }
}

/**
 * Check if a #uiAfterFunc is needed for this button.
 */
static bool ui_afterfunc_check(const uiBlock *block, const uiBut *but)
{
  return (but->func || but->apply_func || but->funcN || but->rename_func ||
          but->rename_full_func || but->optype || but->rnaprop || block->handle_func ||
          (block->handle && block->handle->popup_op));
}

/**
 * These functions are postponed and only executed after all other
 * handling is done, i.e. menus are closed, in order to avoid conflicts
 * with these functions removing the buttons we are working with.
 */
static void ui_apply_but_func(bContext *C, uiBut *but)
{
  uiBlock *block = but->block;
  if (!ui_afterfunc_check(block, but)) {
    return;
  }

  uiAfterFunc *after = ui_afterfunc_new();

  if (but->func && ELEM(but, but->func_arg1, but->func_arg2)) {
    /* exception, this will crash due to removed button otherwise */
    but->func(C, but->func_arg1, but->func_arg2);
  }
  else {
    after->func = but->func;
  }

  after->func_arg1 = but->func_arg1;
  after->func_arg2 = but->func_arg2;

  after->apply_func = but->apply_func;

  after->funcN = but->funcN;
  after->func_argN = (but->func_argN) ? but->func_argN_copy_fn(but->func_argN) : nullptr;
  after->func_argN_free_fn = but->func_argN_free_fn;
  /* but->func_argN_copy_fn is not needed for #uiAfterFunc. */

  after->rename_func = but->rename_func;
  after->rename_arg1 = but->rename_arg1;
  after->rename_orig = but->rename_orig; /* needs free! */

  after->rename_full_func = but->rename_full_func;
  after->rename_full_new = std::move(but->rename_full_new);
  but->rename_full_new = "";

  after->handle_func = block->handle_func;
  after->handle_func_arg = block->handle_func_arg;
  after->retval = but->retval;

  if (block->handle) {
    after->popup_op = block->handle->popup_op;
  }

  if (!but->operator_never_call) {
    after->optype = but->optype;
    after->opcontext = but->opcontext;
    after->opptr = but->opptr;

    but->optype = nullptr;
    but->opcontext = blender::wm::OpCallContext(0);
    but->opptr = nullptr;
  }

  after->rnapoin = but->rnapoin;
  after->rnaprop = but->rnaprop;

  if (but->type == ButType::SearchMenu) {
    uiButSearch *search_but = (uiButSearch *)but;
    after->search_arg_free_fn = search_but->arg_free_fn;
    after->search_arg = search_but->arg;
    search_but->arg_free_fn = nullptr;
    search_but->arg = nullptr;
  }

  if (but->active != nullptr) {
    uiHandleButtonData *data = but->active;
    if (data->custom_interaction_handle != nullptr) {
      after->custom_interaction_callbacks = block->custom_interaction_callbacks;
      after->custom_interaction_handle = data->custom_interaction_handle;

      /* Ensure this callback runs once and last. */
      uiAfterFunc *after_prev = after->prev;
      if (after_prev && (after_prev->custom_interaction_handle == data->custom_interaction_handle))
      {
        after_prev->custom_interaction_handle = nullptr;
        memset(&after_prev->custom_interaction_callbacks,
               0x0,
               sizeof(after_prev->custom_interaction_callbacks));
      }
      else {
        after->custom_interaction_handle->user_count++;
      }
    }
  }

  if (but->context) {
    after->context = *but->context;
  }

  after->drawstr = ui_but_drawstr_without_sep_char(but);
}

/* typically call ui_apply_but_undo(), ui_apply_but_autokey() */
static void ui_apply_but_undo(uiBut *but)
{
  if (!(but->flag & UI_BUT_UNDO)) {
    return;
  }

  /* Skip undo push for buttons in redo panel, see: #134505. */
  const ARegion *region = CTX_wm_region(static_cast<bContext *>(but->block->evil_C));
  if (region->regiontype == RGN_TYPE_HUD) {
    return;
  }

  std::optional<StringRef> str;
  size_t str_len_clip = SIZE_MAX - 1;
  bool skip_undo = false;

  /* define which string to use for undo */
  if (but->type == ButType::Menu) {
    if (!but->drawstr.empty()) {
      str = but->drawstr;
    }
    str_len_clip = ui_but_drawstr_len_without_sep_char(but);
  }
  else if (!but->drawstr.empty()) {
    str = but->drawstr;
    str_len_clip = ui_but_drawstr_len_without_sep_char(but);
  }
  else {
    str = but->tip;
    str_len_clip = ui_but_tip_len_only_first_line(but);
  }

  /* fallback, else we don't get an undo! */
  if (!str || str->is_empty() || str_len_clip == 0) {
    str = "Unknown Action";
    str_len_clip = str->size();
  }

  /* Optionally override undo when undo system doesn't support storing properties. */
  if (but->rnapoin.owner_id) {
    /* Exception for renaming ID data, we always need undo pushes in this case,
     * because undo systems track data by their ID, see: #67002. */
    /* Exception for active shape-key, since changing this in edit-mode updates
     * the shape key from object mode data. */
    if (ELEM(but->rnaprop, &rna_ID_name, &rna_Object_active_shape_key_index)) {
      /* pass */
    }
    else {
      ID *id = but->rnapoin.owner_id;
      if (!ED_undo_is_legacy_compatible_for_property(
              static_cast<bContext *>(but->block->evil_C), id, but->rnapoin))
      {
        skip_undo = true;
      }
    }
  }

  if (skip_undo == false) {
    /* XXX: disable all undo pushes from UI changes from sculpt mode as they cause memfile undo
     * steps to be written which cause lag: #71434. */
    if (BKE_paintmode_get_active_from_context(static_cast<bContext *>(but->block->evil_C)) ==
        PaintMode::Sculpt)
    {
      skip_undo = true;
    }
  }

  if (skip_undo) {
    str = "";
  }

  /* Delayed, after all other functions run, popups are closed, etc. */
  uiAfterFunc *after = ui_afterfunc_new();
  str->copy_utf8_truncated(after->undostr, min_zz(str_len_clip + 1, sizeof(after->undostr)));
}

static void ui_apply_but_autokey(bContext *C, uiBut *but)
{
  Scene *scene = CTX_data_scene(C);

  /* try autokey */
  ui_but_anim_autokey(C, but, scene, BKE_scene_frame_get(scene));

  if (!but->rnaprop) {
    return;
  }

  if (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD) {
    return;
  }

  /* make a little report about what we've done! */
  std::optional<const std::string> str = WM_prop_pystring_assign(
      C, &but->rnapoin, but->rnaprop, but->rnaindex);
  if (!str.has_value()) {
    return;
  }
  BKE_report(CTX_wm_reports(C), RPT_PROPERTY, str.value().c_str());
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);
}

static void ui_apply_but_funcs_after(bContext *C)
{
  /* Copy to avoid recursive calls. */
  ListBase funcs = UIAfterFuncs;
  BLI_listbase_clear(&UIAfterFuncs);

  LISTBASE_FOREACH_MUTABLE (uiAfterFunc *, afterf, &funcs) {
    uiAfterFunc after = *afterf; /* Copy to avoid memory leak on exit(). */
    BLI_remlink(&funcs, afterf);
    MEM_delete(afterf);

    if (after.context) {
      CTX_store_set(C, &after.context.value());
    }

    if (after.popup_op) {
      popup_check(C, after.popup_op);
    }

    PointerRNA opptr;
    if (after.opptr) {
      /* free in advance to avoid leak on exit */
      opptr = *after.opptr;
      MEM_delete(after.opptr);
    }

    if (after.optype) {
      WM_operator_name_call_ptr_with_depends_on_cursor(C,
                                                       after.optype,
                                                       after.opcontext,
                                                       (after.opptr) ? &opptr : nullptr,
                                                       nullptr,
                                                       after.drawstr);
    }

    if (after.opptr) {
      WM_operator_properties_free(&opptr);
    }

    if (after.rnapoin.data) {
      RNA_property_update(C, &after.rnapoin, after.rnaprop);
    }

    if (after.context) {
      CTX_store_set(C, nullptr);
    }

    if (after.rename_full_func) {
      BLI_assert(!after.rename_func);
      after.rename_full_func(after.rename_full_new);
    }

    if (after.func) {
      after.func(C, after.func_arg1, after.func_arg2);
    }
    if (after.apply_func) {
      after.apply_func(*C);
    }
    if (after.funcN) {
      after.funcN(C, after.func_argN, after.func_arg2);
    }
    if (after.func_argN) {
      after.func_argN_free_fn(after.func_argN);
    }

    if (after.handle_func) {
      after.handle_func(C, after.handle_func_arg, after.retval);
    }

    if (after.rename_func) {
      after.rename_func(C, after.rename_arg1, static_cast<char *>(after.rename_orig));
    }
    if (after.rename_orig) {
      MEM_freeN(after.rename_orig);
    }

    if (after.search_arg_free_fn) {
      after.search_arg_free_fn(after.search_arg);
    }

    if (after.custom_interaction_handle != nullptr) {
      after.custom_interaction_handle->user_count--;
      BLI_assert(after.custom_interaction_handle->user_count >= 0);
      if (after.custom_interaction_handle->user_count == 0) {
        ui_block_interaction_update(
            C, &after.custom_interaction_callbacks, after.custom_interaction_handle);
        ui_block_interaction_end(
            C, &after.custom_interaction_callbacks, after.custom_interaction_handle);
      }
      after.custom_interaction_handle = nullptr;
    }

    ui_afterfunc_update_preferences_dirty(&after);

    if (after.undostr[0]) {
      /* Remove "Adjust Last Operation" HUD. Using it would revert this undo push which isn't
       * obvious, see #78171. */
      WM_operator_stack_clear(CTX_wm_manager(C));
      ED_undo_push(C, after.undostr);
    }
  }
}

static void ui_apply_but_BUT(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_BUTM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_but_value_set(but, but->hardmin);
  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_BLOCK(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (but->type == ButType::Menu) {
    ui_but_value_set(but, data->value);
  }

  ui_but_update_edited(but);
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_TOG(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  const double value = ui_but_value_get(but);
  int value_toggle;
  if (but->bit) {
    value_toggle = UI_BITBUT_VALUE_TOGGLED(int(value), but->bitnr);
  }
  else {
    value_toggle = (value == 0.0);
  }

  ui_but_value_set(but, double(value_toggle));
  if (ELEM(but->type, ButType::IconToggle, ButType::IconToggleN)) {
    ui_but_update_edited(but);
  }

  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_ROW(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
  ui_but_value_set(but, but->hardmax);

  ui_apply_but_func(C, but);

  /* states of other row buttons */
  for (const std::unique_ptr<uiBut> &bt : block->buttons) {
    if (bt.get() != but && bt->poin == but->poin && ELEM(bt->type, ButType::Row, ButType::ListRow))
    {
      ui_but_update_edited(bt.get());
    }
  }

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_VIEW_ITEM(bContext *C,
                                   uiBlock *block,
                                   uiBut *but,
                                   uiHandleButtonData *data)
{
  if (data->apply_through_extra_icon) {
    /* Don't apply this, it would cause unintended tree-row toggling when clicking on extra icons.
     */
    return;
  }
  ui_apply_but_ROW(C, block, but, data);
}

static void ui_apply_but_TEX(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (!data->text_edit.edit_string) {
    return;
  }

  ui_but_string_set(C, but, data->text_edit.edit_string);
  ui_but_update_edited(but);

  /* give butfunc a copy of the original text too.
   * feature used for bone renaming, channels, etc.
   * afterfunc frees rename_orig */
  if (data->text_edit.original_string && (but->flag & UI_BUT_TEXTEDIT_UPDATE)) {
    /* In this case, we need to keep `original_string` available,
     * to restore real org string in case we cancel after having typed something already. */
    but->rename_orig = BLI_strdup(data->text_edit.original_string);
  }
  /* only if there are afterfuncs, otherwise 'renam_orig' isn't freed */
  else if (ui_afterfunc_check(but->block, but)) {
    but->rename_orig = data->text_edit.original_string;
    data->text_edit.original_string = nullptr;
  }

  void *orig_arg2 = but->func_arg2;

  /* If arg2 isn't in use already, pass the active search item through it. */
  if ((but->func_arg2 == nullptr) && (but->type == ButType::SearchMenu)) {
    uiButSearch *search_but = (uiButSearch *)but;
    but->func_arg2 = search_but->item_active;
    if ((U.flag & USER_FLAG_RECENT_SEARCHES_DISABLE) == 0) {
      blender::ui::string_search::add_recent_search(search_but->item_active_str);
    }
  }

  ui_apply_but_func(C, but);

  but->func_arg2 = orig_arg2;

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_TAB(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (data->text_edit.edit_string) {
    ui_but_string_set(C, but, data->text_edit.edit_string);
    ui_but_update_edited(but);
  }
  else {
    ui_but_value_set(but, but->hardmax);
    ui_apply_but_func(C, but);
  }

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_NUM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (data->text_edit.edit_string) {
    /* This is intended to avoid unnecessary updates when the value stays the same, however there
     * are issues with the current implementation. It does not work with multi-button editing
     * (#89996) or operator popups where a number button requires an update even if the value is
     * unchanged (#89996).
     *
     * Trying to detect changes at this level is not reliable. Instead it could be done at the
     * level of RNA update/set, skipping RNA update if RNA set did not change anything, instead
     * of skipping all button updates. */
#if 0
    double value;
    /* Check if the string value is a number and cancel if it's equal to the startvalue. */
    if (ui_but_string_eval_number(C, but, data->str, &value) && (value == data->startvalue)) {
      data->cancel = true;
      return;
    }
#endif

    if (ui_but_string_set(C, but, data->text_edit.edit_string)) {
      data->value = ui_but_value_get(but);
    }
    else {
      data->cancel = true;
      return;
    }
  }
  else {
    ui_but_value_set(but, data->value);
  }

  ui_but_update_edited(but);
  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_VEC(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_but_v3_set(but, data->vec);
  ui_but_update_edited(but);
  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_COLORBAND(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_CURVE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_CURVEPROFILE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Drag Multi-Number
 * \{ */

#ifdef USE_DRAG_MULTINUM

/* Small multi-but API. */
static void ui_multibut_add(uiHandleButtonData *data, uiBut *but)
{
  BLI_assert(but->flag & UI_BUT_DRAG_MULTI);
  BLI_assert(data->multi_data.has_mbuts);

  uiButMultiState *mbut_state = MEM_new<uiButMultiState>(__func__);
  mbut_state->but = but;
  mbut_state->origvalue = ui_but_value_get(but);
#  ifdef USE_ALLSELECT
  mbut_state->select_others.is_copy = data->select_others.is_copy;
#  endif

  BLI_linklist_prepend(&data->multi_data.mbuts, mbut_state);

  UI_butstore_register(data->multi_data.bs_mbuts, &mbut_state->but);
}

static uiButMultiState *ui_multibut_lookup(uiHandleButtonData *data, const uiBut *but)
{
  for (LinkNode *l = data->multi_data.mbuts; l; l = l->next) {
    uiButMultiState *mbut_state = static_cast<uiButMultiState *>(l->link);

    if (mbut_state->but == but) {
      return mbut_state;
    }
  }

  return nullptr;
}

static void ui_multibut_restore(bContext *C, uiHandleButtonData *data, uiBlock *block)
{
  for (const std::unique_ptr<uiBut> &but : block->buttons) {
    if (but->flag & UI_BUT_DRAG_MULTI) {
      uiButMultiState *mbut_state = ui_multibut_lookup(data, but.get());
      if (mbut_state) {
        ui_but_value_set(but.get(), mbut_state->origvalue);

#  ifdef USE_ALLSELECT
        if (!mbut_state->select_others.elems.is_empty()) {
          ui_selectcontext_apply(C,
                                 but.get(),
                                 &mbut_state->select_others,
                                 mbut_state->origvalue,
                                 mbut_state->origvalue);
        }
#  else
        UNUSED_VARS(C);
#  endif
      }
    }
  }
}

static void ui_multibut_free(uiHandleButtonData *data, uiBlock *block)
{
#  ifdef USE_ALLSELECT
  if (data->multi_data.mbuts) {
    LinkNode *list = data->multi_data.mbuts;
    while (list) {
      LinkNode *next = list->next;
      uiButMultiState *mbut_state = static_cast<uiButMultiState *>(list->link);
      MEM_delete(mbut_state);
      MEM_freeN(list);
      list = next;
    }
  }
#  else
  BLI_linklist_freeN(data->multi_data.mbuts);
#  endif

  data->multi_data.mbuts = nullptr;

  if (data->multi_data.bs_mbuts) {
    UI_butstore_free(block, data->multi_data.bs_mbuts);
    data->multi_data.bs_mbuts = nullptr;
  }
}

static bool ui_multibut_states_tag(uiBut *but_active,
                                   uiHandleButtonData *data,
                                   const wmEvent *event)
{
  float seg[2][2];
  bool changed = false;

  seg[0][0] = data->multi_data.drag_start[0];
  seg[0][1] = data->multi_data.drag_start[1];

  seg[1][0] = event->xy[0];
  seg[1][1] = event->xy[1];

  BLI_assert(data->multi_data.init == uiHandleButtonMulti::INIT_SETUP);

  ui_window_to_block_fl(data->region, but_active->block, &seg[0][0], &seg[0][1]);
  ui_window_to_block_fl(data->region, but_active->block, &seg[1][0], &seg[1][1]);

  data->multi_data.has_mbuts = false;

  /* follow ui_but_find_mouse_over_ex logic */
  for (const std::unique_ptr<uiBut> &but : but_active->block->buttons) {
    bool drag_prev = false;
    bool drag_curr = false;

    /* re-set each time */
    if (but->flag & UI_BUT_DRAG_MULTI) {
      but->flag &= ~UI_BUT_DRAG_MULTI;
      drag_prev = true;
    }

    if (ui_but_is_interactive(but.get(), false)) {

      /* drag checks */
      if (but_active != but.get()) {
        if (ui_but_is_compatible(but_active, but.get())) {

          BLI_assert(but->active == nullptr);

          /* finally check for overlap */
          if (BLI_rctf_isect_segment(&but->rect, seg[0], seg[1])) {

            but->flag |= UI_BUT_DRAG_MULTI;
            data->multi_data.has_mbuts = true;
            drag_curr = true;
          }
        }
      }
    }

    changed |= (drag_prev != drag_curr);
  }

  return changed;
}

static void ui_multibut_states_create(uiBut *but_active, uiHandleButtonData *data)
{
  BLI_assert(data->multi_data.init == uiHandleButtonMulti::INIT_SETUP);
  BLI_assert(data->multi_data.has_mbuts);

  data->multi_data.bs_mbuts = UI_butstore_create(but_active->block);

  for (const std::unique_ptr<uiBut> &but : but_active->block->buttons) {
    if (but->flag & UI_BUT_DRAG_MULTI) {
      ui_multibut_add(data, but.get());
    }
  }

  /* Edit buttons proportionally to each other.
   * NOTE: if we mix buttons which are proportional and others which are not,
   * this may work a bit strangely. */
  if ((but_active->rnaprop && (RNA_property_flag(but_active->rnaprop) & PROP_PROPORTIONAL)) ||
      ELEM(but_active->unit_type, RNA_SUBTYPE_UNIT_VALUE(PROP_UNIT_LENGTH)))
  {
    if (data->origvalue != 0.0) {
      data->multi_data.is_proportional = true;
    }
  }
}

static void ui_multibut_states_apply(bContext *C, uiHandleButtonData *data, uiBlock *block)
{
  ARegion *region = data->region;
  const double value_delta = data->value - data->origvalue;
  const double value_scale = data->multi_data.is_proportional ? (data->value / data->origvalue) :
                                                                0.0;

  BLI_assert(data->multi_data.init == uiHandleButtonMulti::INIT_ENABLE);
  BLI_assert(data->multi_data.skip == false);

  for (const std::unique_ptr<uiBut> &but : block->buttons) {
    if (!(but->flag & UI_BUT_DRAG_MULTI)) {
      continue;
    }

    uiButMultiState *mbut_state = ui_multibut_lookup(data, but.get());

    if (mbut_state == nullptr) {
      /* Highly unlikely. */
      CLOG_WARN(&LOG, "%s: Can't find button", __func__);
      /* While this avoids crashing, multi-button dragging will fail,
       * which is still a bug from the user perspective. See #83651. */
      continue;
    }

    void *active_back;
    ui_but_execute_begin(C, region, but.get(), &active_back);

#  ifdef USE_ALLSELECT
    if (data->select_others.is_enabled) {
      /* init once! */
      if (mbut_state->select_others.elems.is_empty()) {
        ui_selectcontext_begin(C, but.get(), &mbut_state->select_others);
      }
      if (mbut_state->select_others.elems.is_empty()) {
        mbut_state->select_others.elems.clear();
      }
    }

    /* Needed so we apply the right deltas. */
    but->active->origvalue = mbut_state->origvalue;
    but->active->select_others = mbut_state->select_others;
    but->active->select_others.do_free = false;
#  endif

    BLI_assert(active_back == nullptr);
    /* No need to check 'data->state' here. */
    if (data->text_edit.edit_string) {
      /* Entering text (set all). */
      but->active->value = data->value;
      ui_but_string_set(C, but.get(), data->text_edit.edit_string);
    }
    else {
      /* Dragging (use delta). */
      if (data->multi_data.is_proportional) {
        but->active->value = mbut_state->origvalue * value_scale;
      }
      else {
        but->active->value = mbut_state->origvalue + value_delta;
      }

      /* Clamp based on soft limits, see #40154. */
      CLAMP(but->active->value, double(but->softmin), double(but->softmax));
    }

    ui_but_execute_end(C, region, but.get(), active_back);
  }
}

#endif /* USE_DRAG_MULTINUM */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Drag Toggle
 * \{ */

#ifdef USE_DRAG_TOGGLE

/* Helpers that wrap boolean functions, to support different kinds of buttons. */

static bool ui_drag_toggle_but_is_supported(const uiBut *but)
{
  if (but->flag & UI_BUT_DISABLED) {
    return false;
  }
  if (ui_but_is_bool(but)) {
    return true;
  }
  if (UI_but_is_decorator(but)) {
    const uiButDecorator *but_decorate = static_cast<const uiButDecorator *>(but);
    return but_decorate->toggle_keyframe_on_click;
  }
  return false;
}

/* Button pushed state to compare if other buttons match. Can be more
 * then just true or false for toggle buttons with more than 2 states. */
static int ui_drag_toggle_but_pushed_state(uiBut *but)
{
  if (but->rnapoin.data == nullptr && but->poin == nullptr && but->icon) {
    /* Assume icon identifies a unique state, for buttons that
     * work through functions callbacks and don't have an boolean
     * value that indicates the state. */
    return but->icon + but->iconadd;
  }
  if (ui_but_is_bool(but)) {
    return ui_but_is_pushed(but);
  }
  return 0;
}

struct uiDragToggleHandle {
  /* init */
  int pushed_state;
  float but_cent_start[2];

  bool is_xy_lock_init;
  bool xy_lock[2];

  int xy_init[2];
  int xy_last[2];
};

static bool ui_drag_toggle_set_xy_xy(
    bContext *C, ARegion *region, const int pushed_state, const int xy_src[2], const int xy_dst[2])
{
  /* popups such as layers won't re-evaluate on redraw */
  const bool do_check = (region->regiontype == RGN_TYPE_TEMPORARY);
  bool changed = false;

  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    float xy_a_block[2] = {float(xy_src[0]), float(xy_src[1])};
    float xy_b_block[2] = {float(xy_dst[0]), float(xy_dst[1])};

    ui_window_to_block_fl(region, block, &xy_a_block[0], &xy_a_block[1]);
    ui_window_to_block_fl(region, block, &xy_b_block[0], &xy_b_block[1]);

    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      /* NOTE: ctrl is always true here because (at least for now)
       * we always want to consider text control in this case, even when not embossed. */

      if (!ui_but_is_interactive(but.get(), true)) {
        continue;
      }
      if (!BLI_rctf_isect_segment(&but->rect, xy_a_block, xy_b_block)) {
        continue;
      }
      if (!ui_drag_toggle_but_is_supported(but.get())) {
        continue;
      }
      /* is it pressed? */
      const int pushed_state_but = ui_drag_toggle_but_pushed_state(but.get());
      if (pushed_state_but == pushed_state) {
        continue;
      }

      /* execute the button */
      UI_but_execute(C, region, but.get());
      if (do_check) {
        ui_but_update_edited(but.get());
      }
      if (U.runtime.is_dirty == false) {
        ui_but_update_preferences_dirty(but.get());
      }
      changed = true;
    }
  }

  if (changed) {
    /* apply now, not on release (or if handlers are canceled for whatever reason) */
    ui_apply_but_funcs_after(C);
  }

  return changed;
}

static void ui_drag_toggle_set(bContext *C, uiDragToggleHandle *drag_info, const int xy_input[2])
{
  ARegion *region = CTX_wm_region(C);
  bool do_draw = false;

  /**
   * Initialize Locking:
   *
   * Check if we need to initialize the lock axis by finding if the first
   * button we mouse over is X or Y aligned, then lock the mouse to that axis after.
   */
  if (drag_info->is_xy_lock_init == false) {
    /* first store the buttons original coords */
    uiBut *but = ui_but_find_mouse_over_ex(region, xy_input, true, false, nullptr, nullptr);

    if (but) {
      if (but->flag & UI_BUT_DRAG_LOCK) {
        const float but_cent_new[2] = {
            BLI_rctf_cent_x(&but->rect),
            BLI_rctf_cent_y(&but->rect),
        };

        /* check if this is a different button,
         * chances are high the button won't move about :) */
        if (len_manhattan_v2v2(drag_info->but_cent_start, but_cent_new) > 1.0f) {
          if (fabsf(drag_info->but_cent_start[0] - but_cent_new[0]) <
              fabsf(drag_info->but_cent_start[1] - but_cent_new[1]))
          {
            drag_info->xy_lock[0] = true;
          }
          else {
            drag_info->xy_lock[1] = true;
          }
          drag_info->is_xy_lock_init = true;
        }
      }
      else {
        drag_info->is_xy_lock_init = true;
      }
    }
  }
  /* done with axis locking */

  int xy[2];
  xy[0] = (drag_info->xy_lock[0] == false) ? xy_input[0] : drag_info->xy_last[0];
  xy[1] = (drag_info->xy_lock[1] == false) ? xy_input[1] : drag_info->xy_last[1];

  /* touch all buttons between last mouse coord and this one */
  do_draw = ui_drag_toggle_set_xy_xy(C, region, drag_info->pushed_state, drag_info->xy_last, xy);

  if (do_draw) {
    ED_region_tag_redraw(region);
  }

  copy_v2_v2_int(drag_info->xy_last, xy);
}

static void ui_handler_region_drag_toggle_remove(bContext * /*C*/, void *userdata)
{
  uiDragToggleHandle *drag_info = static_cast<uiDragToggleHandle *>(userdata);
  MEM_freeN(drag_info);
}

static int ui_handler_region_drag_toggle(bContext *C, const wmEvent *event, void *userdata)
{
  uiDragToggleHandle *drag_info = static_cast<uiDragToggleHandle *>(userdata);
  bool done = false;

  switch (event->type) {
    case LEFTMOUSE: {
      if (event->val == KM_RELEASE) {
        done = true;
      }
      break;
    }
    case MOUSEMOVE: {
      ui_drag_toggle_set(C, drag_info, event->xy);
      break;
    }
    default: {
      break;
    }
  }

  if (done) {
    wmWindow *win = CTX_wm_window(C);
    const ARegion *region = CTX_wm_region(C);
    uiBut *but = ui_but_find_mouse_over_ex(
        region, drag_info->xy_init, true, false, nullptr, nullptr);

    if (but) {
      ui_apply_but_undo(but);
    }

    WM_event_remove_ui_handler(&win->modalhandlers,
                               ui_handler_region_drag_toggle,
                               ui_handler_region_drag_toggle_remove,
                               drag_info,
                               false);
    ui_handler_region_drag_toggle_remove(C, drag_info);

    WM_event_add_mousemove(win);
    return WM_UI_HANDLER_BREAK;
  }
  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_but_is_drag_toggle(const uiBut *but)
{
  return ((ui_drag_toggle_but_is_supported(but) == true) &&
          /* Menu check is important so the button dragged over isn't removed instantly. */
          (ui_block_is_menu(but->block) == false));
}

#endif /* USE_DRAG_TOGGLE */

#ifdef USE_ALLSELECT

static bool ui_selectcontext_begin(bContext *C, uiBut *but, uiSelectContextStore *selctx_data)
{
  PointerRNA lptr;
  PropertyRNA *lprop;
  bool success = false;

  blender::Vector<PointerRNA> lb;

  PointerRNA ptr = but->rnapoin;
  PropertyRNA *prop = but->rnaprop;
  const int index = but->rnaindex;

  /* for now don't support whole colors */
  if (index == -1) {
    return false;
  }

  /* if there is a valid property that is editable... */
  if (ptr.data && prop) {
    bool use_path_from_id;

    /* some facts we want to know */
    const bool is_array = RNA_property_array_check(prop);
    const int rna_type = RNA_property_type(prop);

    std::optional<std::string> path;
    if (UI_context_copy_to_selected_list(C, &ptr, prop, &lb, &use_path_from_id, &path) &&
        !lb.is_empty())
    {
      selctx_data->elems.resize(lb.size());

      int i;
      PointerRNA *link;
      for (i = 0, link = lb.data(); i < selctx_data->elems.size(); i++, link++) {
        if (!UI_context_copy_to_selected_check(&ptr,
                                               link,
                                               prop,
                                               path.has_value() ? path->c_str() : nullptr,
                                               use_path_from_id,
                                               &lptr,
                                               &lprop))
        {
          selctx_data->elems.pop_last();
          i -= 1;
          continue;
        }

        uiSelectContextElem *other = &selctx_data->elems[i];
        other->ptr = lptr;
        if (is_array) {
          if (rna_type == PROP_FLOAT) {
            other->val_f = RNA_property_float_get_index(&lptr, lprop, index);
          }
          else if (rna_type == PROP_INT) {
            other->val_i = RNA_property_int_get_index(&lptr, lprop, index);
          }
          /* ignored for now */
#  if 0
          else if (rna_type == PROP_BOOLEAN) {
            other->val_b = RNA_property_boolean_get_index(&lptr, lprop, index);
          }
#  endif
        }
        else {
          if (rna_type == PROP_FLOAT) {
            other->val_f = RNA_property_float_get(&lptr, lprop);
          }
          else if (rna_type == PROP_INT) {
            other->val_i = RNA_property_int_get(&lptr, lprop);
          }
          /* ignored for now */
#  if 0
          else if (rna_type == PROP_BOOLEAN) {
            other->val_b = RNA_property_boolean_get(&lptr, lprop);
          }
          else if (rna_type == PROP_ENUM) {
            other->val_i = RNA_property_enum_get(&lptr, lprop);
          }
#  endif
        }
      }
      success = !selctx_data->elems.is_empty();
    }
  }

  /* caller can clear */
  selctx_data->do_free = true;

  if (success) {
    but->flag |= UI_BUT_IS_SELECT_CONTEXT;
  }

  return success;
}

static void ui_selectcontext_end(uiBut *but, uiSelectContextStore *selctx_data)
{
  if (selctx_data->do_free) {
    selctx_data->elems.clear();
  }

  but->flag &= ~UI_BUT_IS_SELECT_CONTEXT;
}

static void ui_selectcontext_apply(bContext *C,
                                   uiBut *but,
                                   uiSelectContextStore *selctx_data,
                                   const double value,
                                   const double value_orig)
{
  if (!selctx_data->elems.is_empty()) {
    PropertyRNA *prop = but->rnaprop;
    PropertyRNA *lprop = but->rnaprop;
    const int index = but->rnaindex;
    const bool use_delta = (selctx_data->is_copy == false);

    std::variant<bool, int, float, std::string, PointerRNA> delta, min, max;

    const bool is_array = RNA_property_array_check(prop);
    const int rna_type = RNA_property_type(prop);

    if (rna_type == PROP_FLOAT) {
      delta.emplace<float>(use_delta ? (value - value_orig) : value);
      float min_v, max_v;
      RNA_property_float_range(&but->rnapoin, prop, &min_v, &max_v);
      min.emplace<float>(min_v);
      max.emplace<float>(max_v);
    }
    else if (rna_type == PROP_INT) {
      delta.emplace<int>(int(use_delta ? (value - value_orig) : value));
      int min_v, max_v;
      RNA_property_int_range(&but->rnapoin, prop, &min_v, &max_v);
      min.emplace<int>(min_v);
      max.emplace<int>(max_v);
    }
    else if (rna_type == PROP_ENUM) {
      /* Not a delta in fact. */
      delta.emplace<int>(RNA_property_enum_get(&but->rnapoin, prop));
    }
    else if (rna_type == PROP_BOOLEAN) {
      if (is_array) {
        /* Not a delta in fact. */
        delta.emplace<bool>(RNA_property_boolean_get_index(&but->rnapoin, prop, index));
      }
      else {
        /* Not a delta in fact. */
        delta.emplace<bool>(RNA_property_boolean_get(&but->rnapoin, prop));
      }
    }
    else if (rna_type == PROP_POINTER) {
      /* Not a delta in fact. */
      delta.emplace<PointerRNA>(RNA_property_pointer_get(&but->rnapoin, prop));
    }
    else if (rna_type == PROP_STRING) {
      /* Not a delta in fact. */
      delta.emplace<std::string>(RNA_property_string_get(&but->rnapoin, prop));
    }

#  ifdef USE_ALLSELECT_LAYER_HACK
    /* make up for not having 'handle_layer_buttons' */
    {
      const PropertySubType subtype = RNA_property_subtype(prop);

      if ((rna_type == PROP_BOOLEAN) && ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER) && is_array &&
          /* could check for 'handle_layer_buttons' */
          but->func)
      {
        wmWindow *win = CTX_wm_window(C);
        if ((win->eventstate->modifier & KM_SHIFT) == 0) {
          const int len = RNA_property_array_length(&but->rnapoin, prop);
          bool *tmparray = MEM_calloc_arrayN<bool>(len, __func__);

          tmparray[index] = true;

          for (uiSelectContextElem &other : selctx_data->elems) {
            PointerRNA lptr = other.ptr;
            RNA_property_boolean_set_array(&lptr, lprop, tmparray);
            RNA_property_update(C, &lptr, lprop);
          }

          MEM_freeN(tmparray);

          return;
        }
      }
    }
#  endif

    for (uiSelectContextElem &other : selctx_data->elems) {
      PointerRNA lptr = other.ptr;

      if (rna_type == PROP_FLOAT) {
        float other_value = std::get<float>(delta) + (use_delta ? other.val_f : 0.0f);
        CLAMP(other_value, std::get<float>(min), std::get<float>(max));
        if (is_array) {
          RNA_property_float_set_index(&lptr, lprop, index, other_value);
        }
        else {
          RNA_property_float_set(&lptr, lprop, other_value);
        }
      }
      else if (rna_type == PROP_INT) {
        int other_value = std::get<int>(delta) + (use_delta ? other.val_i : 0);
        CLAMP(other_value, std::get<int>(min), std::get<int>(max));
        if (is_array) {
          RNA_property_int_set_index(&lptr, lprop, index, other_value);
        }
        else {
          RNA_property_int_set(&lptr, lprop, other_value);
        }
      }
      else if (rna_type == PROP_BOOLEAN) {
        const bool other_value = std::get<bool>(delta);
        if (is_array) {
          RNA_property_boolean_set_index(&lptr, lprop, index, other_value);
        }
        else {
          RNA_property_boolean_set(&lptr, lprop, other_value);
        }
      }
      else if (rna_type == PROP_ENUM) {
        const int other_value = std::get<int>(delta);
        BLI_assert(!is_array);
        RNA_property_enum_set(&lptr, lprop, other_value);
      }
      else if (rna_type == PROP_POINTER) {
        const PointerRNA &other_value = std::get<PointerRNA>(delta);
        RNA_property_pointer_set(&lptr, lprop, other_value, nullptr);
      }
      else if (rna_type == PROP_STRING) {
        const std::string &other_value = std::get<std::string>(delta);
        RNA_property_string_set(&lptr, lprop, other_value.c_str());
      }

      RNA_property_update(C, &lptr, prop);
    }
  }
}

#endif /* USE_ALLSELECT */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Drag
 * \{ */

static bool ui_but_drag_init(bContext *C,
                             uiBut *but,
                             uiHandleButtonData *data,
                             const wmEvent *event)
{
  /* prevent other WM gestures to start while we try to drag */
  WM_gestures_remove(CTX_wm_window(C));

  /* Clamp the maximum to half the UI unit size so a high user preference
   * doesn't require the user to drag more than half the default button height. */
  const int drag_threshold = min_ii(
      WM_event_drag_threshold(event),
      int((UI_UNIT_Y / 2) * ui_block_to_window_scale(data->region, but->block)));

  if (abs(data->dragstartx - event->xy[0]) + abs(data->dragstarty - event->xy[1]) > drag_threshold)
  {
    button_activate_state(C, but, BUTTON_STATE_EXIT);
    data->cancel = true;
#ifdef USE_DRAG_TOGGLE
    if (ui_drag_toggle_but_is_supported(but)) {
      uiDragToggleHandle *drag_info = MEM_callocN<uiDragToggleHandle>(__func__);
      ARegion *region_prev;

      /* call here because regular mouse-up event won't run,
       * typically 'button_activate_exit()' handles this */
      ui_apply_but_autokey(C, but);

      drag_info->pushed_state = ui_drag_toggle_but_pushed_state(but);
      drag_info->but_cent_start[0] = BLI_rctf_cent_x(&but->rect);
      drag_info->but_cent_start[1] = BLI_rctf_cent_y(&but->rect);
      copy_v2_v2_int(drag_info->xy_init, event->xy);
      copy_v2_v2_int(drag_info->xy_last, event->xy);

      /* needed for toggle drag on popups */
      region_prev = CTX_wm_region(C);
      CTX_wm_region_set(C, data->region);

      WM_event_add_ui_handler(C,
                              &data->window->modalhandlers,
                              ui_handler_region_drag_toggle,
                              ui_handler_region_drag_toggle_remove,
                              drag_info,
                              WM_HANDLER_BLOCKING);

      CTX_wm_region_set(C, region_prev);

      /* Initialize alignment for single row/column regions,
       * otherwise we use the relative position of the first other button dragged over. */
      if (ELEM(data->region->regiontype,
               RGN_TYPE_NAV_BAR,
               RGN_TYPE_HEADER,
               RGN_TYPE_TOOL_HEADER,
               RGN_TYPE_FOOTER,
               RGN_TYPE_ASSET_SHELF_HEADER))
      {
        const int region_alignment = RGN_ALIGN_ENUM_FROM_MASK(data->region->alignment);
        int lock_axis = -1;

        if (ELEM(region_alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
          lock_axis = 0;
        }
        else if (ELEM(region_alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
          lock_axis = 1;
        }
        if (lock_axis != -1) {
          drag_info->xy_lock[lock_axis] = true;
          drag_info->is_xy_lock_init = true;
        }
      }
    }
    else
#endif
        if (but->type == ButType::Color)
    {
      bool valid = false;
      uiDragColorHandle *drag_info = MEM_callocN<uiDragColorHandle>(__func__);

      drag_info->has_alpha = ui_but_color_has_alpha(but);

      /* TODO: support more button pointer types. */
      if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
        ui_but_v4_get(but, drag_info->color);
        drag_info->gamma_corrected = true;
        valid = true;
      }
      else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
        ui_but_v4_get(but, drag_info->color);
        drag_info->gamma_corrected = false;
        valid = true;
      }
      else if (ELEM(but->pointype, ButPointerType::Float, ButPointerType::Char)) {
        ui_but_v4_get(but, drag_info->color);
        copy_v4_v4(drag_info->color, (float *)but->poin);
        drag_info->gamma_corrected = false;
        valid = true;
      }

      if (valid) {
        WM_event_start_drag(C, ICON_COLOR, WM_DRAG_COLOR, drag_info, WM_DRAG_FREE_DATA);
      }
      else {
        MEM_freeN(drag_info);
        return false;
      }
    }
    else if (but->type == ButType::ViewItem) {
      const uiButViewItem *view_item_but = (uiButViewItem *)but;
      if (view_item_but->view_item) {
        return UI_view_item_drag_start(*C, *view_item_but->view_item);
      }
    }
    else {
      ui_but_drag_start(C, but);
    }
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Apply
 * \{ */

static void ui_apply_but_IMAGE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_HISTOGRAM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_WAVEFORM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_TRACKPREVIEW(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  ui_apply_but_func(C, but);
  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const bool interactive)
{
  const ButType but_type = but->type; /* Store as const to quiet maybe uninitialized warning. */

  data->retval = 0;

  /* if we cancel and have not applied yet, there is nothing to do,
   * otherwise we have to restore the original value again */
  if (data->cancel) {
    if (!data->applied) {
      return;
    }

    if (data->text_edit.edit_string) {
      MEM_freeN(data->text_edit.edit_string);
    }
    data->text_edit.edit_string = data->text_edit.original_string;
    data->text_edit.original_string = nullptr;
    data->value = data->origvalue;
    copy_v3_v3(data->vec, data->origvec);
    /* postpone clearing origdata */
  }
  else {
    /* We avoid applying interactive edits a second time
     * at the end with the #uiHandleButtonData.applied_interactive flag. */
    if (interactive) {
      data->applied_interactive = true;
    }
    else if (data->applied_interactive) {
      return;
    }

#ifdef USE_ALLSELECT
#  ifdef USE_DRAG_MULTINUM
    if (but->flag & UI_BUT_DRAG_MULTI) {
      /* pass */
    }
    else
#  endif
        if (data->select_others.elems.is_empty())
    {
      wmWindow *win = CTX_wm_window(C);
      const wmEvent *event = win->eventstate;
      /* May have been enabled before activating, don't do for array pasting. */
      if (data->select_others.is_enabled || IS_ALLSELECT_EVENT(event)) {
        /* See comment for #IS_ALLSELECT_EVENT why this needs to be filtered here. */
        const bool is_array_paste = (event->val == KM_PRESS) &&
                                    (event->modifier & (KM_CTRL | KM_OSKEY)) &&
                                    (event->modifier & KM_SHIFT) == 0 && (event->type == EVT_VKEY);
        if (!is_array_paste) {
          ui_selectcontext_begin(C, but, &data->select_others);
          data->select_others.is_enabled = true;
        }
      }
    }
    if (data->select_others.elems.is_empty()) {
      /* Don't check again. */
      data->select_others.elems.clear();
    }
#endif
  }

  /* ensures we are writing actual values */
  char *editstr = but->editstr;
  double *editval = but->editval;
  float *editvec = but->editvec;
  ColorBand *editcoba;
  CurveMapping *editcumap;
  CurveProfile *editprofile;
  if (but_type == ButType::ColorBand) {
    uiButColorBand *but_coba = (uiButColorBand *)but;
    editcoba = but_coba->edit_coba;
  }
  else if (but_type == ButType::Curve) {
    uiButCurveMapping *but_cumap = (uiButCurveMapping *)but;
    editcumap = but_cumap->edit_cumap;
  }
  else if (but_type == ButType::CurveProfile) {
    uiButCurveProfile *but_profile = (uiButCurveProfile *)but;
    editprofile = but_profile->edit_profile;
  }
  but->editstr = nullptr;
  but->editval = nullptr;
  but->editvec = nullptr;
  if (but_type == ButType::ColorBand) {
    uiButColorBand *but_coba = (uiButColorBand *)but;
    but_coba->edit_coba = nullptr;
  }
  else if (but_type == ButType::Curve) {
    uiButCurveMapping *but_cumap = (uiButCurveMapping *)but;
    but_cumap->edit_cumap = nullptr;
  }
  else if (but_type == ButType::CurveProfile) {
    uiButCurveProfile *but_profile = (uiButCurveProfile *)but;
    but_profile->edit_profile = nullptr;
  }

  /* handle different types */
  switch (but_type) {
    case ButType::But:
    case ButType::Decorator:
    case ButType::PreviewTile:
      ui_apply_but_BUT(C, but, data);
      break;
    case ButType::Text:
    case ButType::SearchMenu:
      ui_apply_but_TEX(C, but, data);
      break;
    case ButType::ButToggle:
    case ButType::Toggle:
    case ButType::ToggleN:
    case ButType::IconToggle:
    case ButType::IconToggleN:
    case ButType::Checkbox:
    case ButType::CheckboxN:
      ui_apply_but_TOG(C, but, data);
      break;
    case ButType::ListRow:
    case ButType::Row:
      ui_apply_but_ROW(C, block, but, data);
      break;
    case ButType::ViewItem:
      ui_apply_but_VIEW_ITEM(C, block, but, data);
      break;
    case ButType::Tab:
      ui_apply_but_TAB(C, but, data);
      break;
    case ButType::Scroll:
    case ButType::Grip:
    case ButType::Num:
    case ButType::NumSlider:
      ui_apply_but_NUM(C, but, data);
      break;
    case ButType::Menu:
    case ButType::Block:
    case ButType::Pulldown:
      ui_apply_but_BLOCK(C, but, data);
      break;
    case ButType::Color:
      if (data->cancel) {
        ui_apply_but_VEC(C, but, data);
      }
      else {
        ui_apply_but_BLOCK(C, but, data);
      }
      break;
    case ButType::ButMenu:
      ui_apply_but_BUTM(C, but, data);
      break;
    case ButType::Unitvec:
    case ButType::HsvCube:
    case ButType::HsvCircle:
      ui_apply_but_VEC(C, but, data);
      break;
    case ButType::ColorBand:
      ui_apply_but_COLORBAND(C, but, data);
      break;
    case ButType::Curve:
      ui_apply_but_CURVE(C, but, data);
      break;
    case ButType::CurveProfile:
      ui_apply_but_CURVEPROFILE(C, but, data);
      break;
    case ButType::KeyEvent:
    case ButType::HotkeyEvent:
      ui_apply_but_BUT(C, but, data);
      break;
    case ButType::Image:
      ui_apply_but_IMAGE(C, but, data);
      break;
    case ButType::Histogram:
      ui_apply_but_HISTOGRAM(C, but, data);
      break;
    case ButType::Waveform:
      ui_apply_but_WAVEFORM(C, but, data);
      break;
    case ButType::TrackPreview:
      ui_apply_but_TRACKPREVIEW(C, but, data);
      break;
    default:
      break;
  }

#ifdef USE_DRAG_MULTINUM
  if (data->multi_data.has_mbuts) {
    if ((data->multi_data.init == uiHandleButtonMulti::INIT_ENABLE) &&
        (data->multi_data.skip == false))
    {
      if (data->cancel) {
        ui_multibut_restore(C, data, block);
      }
      else {
        ui_multibut_states_apply(C, data, block);
      }
    }
  }
#endif

#ifdef USE_ALLSELECT
  ui_selectcontext_apply(C, but, &data->select_others, data->value, data->origvalue);
#endif

  if (data->cancel) {
    data->origvalue = 0.0;
    zero_v3(data->origvec);
  }

  but->editstr = editstr;
  but->editval = editval;
  but->editvec = editvec;
  if (but_type == ButType::ColorBand) {
    uiButColorBand *but_coba = (uiButColorBand *)but;
    but_coba->edit_coba = editcoba;
  }
  else if (but_type == ButType::Curve) {
    uiButCurveMapping *but_cumap = (uiButCurveMapping *)but;
    but_cumap->edit_cumap = editcumap;
  }
  else if (but_type == ButType::CurveProfile) {
    uiButCurveProfile *but_profile = (uiButCurveProfile *)but;
    but_profile->edit_profile = editprofile;
  }

  if (data->custom_interaction_handle != nullptr) {
    ui_block_interaction_update(
        C, &block->custom_interaction_callbacks, data->custom_interaction_handle);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Copy & Paste
 * \{ */

static void ui_but_get_pasted_text_from_clipboard(const bool ensure_utf8,
                                                  char **r_buf_paste,
                                                  int *r_buf_len)
{
  /* get only first line even if the clipboard contains multiple lines */
  int length;
  char *text = WM_clipboard_text_get_firstline(false, ensure_utf8, &length);

  if (text) {
    *r_buf_paste = text;
    *r_buf_len = length;
  }
  else {
    *r_buf_paste = MEM_callocN<char>(__func__);
    *r_buf_len = 0;
  }
}

static int get_but_property_array_length(uiBut *but)
{
  return RNA_property_array_length(&but->rnapoin, but->rnaprop);
}

static void ui_but_set_float_array(
    bContext *C, uiBut *but, uiHandleButtonData *data, const float *values, const int values_len)
{
  button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

  RNA_property_float_set_array_at_most(&but->rnapoin, but->rnaprop, values, values_len);
  if (data) {
    if (but->type == ButType::Unitvec) {
      BLI_assert(values_len == 3);
      copy_v3_v3(data->vec, values);
    }
    else {
      data->value = values[but->rnaindex];
    }
  }

  button_activate_state(C, but, BUTTON_STATE_EXIT);
}

static void float_array_to_string(const float *values,
                                  const int values_len,
                                  char *output,
                                  int output_maxncpy)
{
  const int values_end = values_len - 1;
  int ofs = 0;
  output[ofs++] = '[';
  for (int i = 0; i < values_len; i++) {
    ofs += BLI_snprintf_utf8_rlen(
        output + ofs, output_maxncpy - ofs, (i != values_end) ? "%f, " : "%f]", values[i]);
  }
}

static void ui_but_copy_numeric_array(uiBut *but, char *output, int output_maxncpy)
{
  const int values_len = get_but_property_array_length(but);
  blender::Array<float, 16> values(values_len);
  RNA_property_float_get_array(&but->rnapoin, but->rnaprop, values.data());
  float_array_to_string(values.data(), values_len, output, output_maxncpy);
}

static bool parse_float_array(char *text, float *values, int values_len_expected)
{
  /* can parse max 4 floats for now */
  BLI_assert(0 <= values_len_expected && values_len_expected <= 4);

  float v[5];
  const int values_len_actual = sscanf(
      text, "[%f, %f, %f, %f, %f]", &v[0], &v[1], &v[2], &v[3], &v[4]);

  if (values_len_actual == values_len_expected) {
    memcpy(values, v, sizeof(float) * values_len_expected);
    return true;
  }
  return false;
}

static void ui_but_paste_numeric_array(bContext *C,
                                       uiBut *but,
                                       uiHandleButtonData *data,
                                       char *buf_paste)
{
  const int values_len = get_but_property_array_length(but);
  if (values_len > 4) {
    /* not supported for now */
    return;
  }

  blender::Array<float, 16> values(values_len);

  if (parse_float_array(buf_paste, values.data(), values_len)) {
    ui_but_set_float_array(C, but, data, values.data(), values_len);
  }
  else {
    WM_global_report(RPT_ERROR, "Expected an array of numbers: [n, n, ...]");
  }
}

static void ui_but_copy_numeric_value(uiBut *but, char *output, int output_maxncpy)
{
  /* Get many decimal places, then strip trailing zeros.
   * NOTE: too high values start to give strange results. */
  ui_but_string_get_ex(but, output, output_maxncpy, UI_PRECISION_FLOAT_MAX, false, nullptr);
  BLI_str_rstrip_float_zero(output, '\0');
}

static void ui_but_paste_numeric_value(bContext *C,
                                       uiBut *but,
                                       uiHandleButtonData *data,
                                       char *buf_paste)
{
  double value;
  if (ui_but_string_eval_number(C, but, buf_paste, &value)) {
    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    data->value = value;
    ui_but_string_set(C, but, buf_paste);
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
  else {
    WM_global_report(RPT_ERROR, "Expected a number");
  }
}

static void ui_but_paste_normalized_vector(bContext *C,
                                           uiBut *but,
                                           uiHandleButtonData *data,
                                           char *buf_paste)
{
  float xyz[3];
  if (parse_float_array(buf_paste, xyz, 3)) {
    if (normalize_v3(xyz) == 0.0f) {
      /* better set Z up then have a zero vector */
      xyz[2] = 1.0;
    }
    ui_but_set_float_array(C, but, data, xyz, 3);
  }
  else {
    WM_global_report(RPT_ERROR, "Paste expected 3 numbers, formatted: '[n, n, n]'");
  }
}

static void ui_but_copy_color(uiBut *but, char *output, int output_maxncpy)
{
  float rgba[4];

  if (but->rnaprop && get_but_property_array_length(but) >= 4) {
    rgba[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
  }
  else {
    rgba[3] = 1.0f;
  }

  ui_but_v3_get(but, rgba);

  /* convert to linear color to do compatible copy between gamma and non-gamma */
  if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
    srgb_to_linearrgb_v3_v3(rgba, rgba);
  }

  float_array_to_string(rgba, 4, output, output_maxncpy);
}

static void ui_but_paste_color(bContext *C, uiBut *but, char *buf_paste)
{
  float rgba[4];
  if (parse_float_array(buf_paste, rgba, 4)) {
    if (but->rnaprop) {
      /* Assume linear colors in buffer. */
      if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
        linearrgb_to_srgb_v3_v3(rgba, rgba);
      }

      /* Some color properties are RGB, not RGBA. */
      const int array_len = get_but_property_array_length(but);
      ui_but_set_float_array(C, but, nullptr, rgba, std::min(array_len, int(ARRAY_SIZE(rgba))));
    }
  }
  else {
    WM_global_report(RPT_ERROR, "Paste expected 4 numbers, formatted: '[n, n, n, n]'");
  }
}

static void ui_but_copy_text(uiBut *but, char *output, int output_maxncpy)
{
  ui_but_string_get(but, output, output_maxncpy);
}

static void ui_but_paste_text(bContext *C, uiBut *but, uiHandleButtonData *data, char *buf_paste)
{
  BLI_assert(but->active == data);
  UNUSED_VARS_NDEBUG(data);
  ui_but_set_string_interactive(C, but, buf_paste);
}

static void ui_but_copy_colorband(uiBut *but)
{
  if (but->poin != nullptr) {
    memcpy(&but_copypaste_coba, but->poin, sizeof(ColorBand));
  }
}

static void ui_but_paste_colorband(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (but_copypaste_coba.tot != 0 && but->poin != nullptr) {
    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    memcpy(data->coba, &but_copypaste_coba, sizeof(ColorBand));
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
}

static void ui_but_copy_curvemapping(uiBut *but)
{
  if (but->poin != nullptr) {
    but_copypaste_curve_alive = true;
    BKE_curvemapping_free_data(&but_copypaste_curve);
    BKE_curvemapping_copy_data(&but_copypaste_curve, (CurveMapping *)but->poin);
  }
}

static void ui_but_paste_curvemapping(bContext *C, uiBut *but)
{
  if (but_copypaste_curve_alive && but->poin != nullptr) {
    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    BKE_curvemapping_free_data((CurveMapping *)but->poin);
    BKE_curvemapping_copy_data((CurveMapping *)but->poin, &but_copypaste_curve);
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
}

static void ui_but_copy_CurveProfile(uiBut *but)
{
  if (but->poin != nullptr) {
    but_copypaste_profile_alive = true;
    BKE_curveprofile_free_data(&but_copypaste_profile);
    BKE_curveprofile_copy_data(&but_copypaste_profile, (CurveProfile *)but->poin);
  }
}

static void ui_but_paste_CurveProfile(bContext *C, uiBut *but)
{
  if (but_copypaste_profile_alive && but->poin != nullptr) {
    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    BKE_curveprofile_free_data((CurveProfile *)but->poin);
    BKE_curveprofile_copy_data((CurveProfile *)but->poin, &but_copypaste_profile);
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
}

static void ui_but_copy_operator(bContext *C, uiBut *but, char *output, int output_maxncpy)
{
  PointerRNA *opptr = UI_but_operator_ptr_ensure(but);

  std::string str = WM_operator_pystring_ex(C, nullptr, false, true, but->optype, opptr);
  BLI_strncpy_utf8(output, str.c_str(), output_maxncpy);
}

static bool ui_but_copy_menu(uiBut *but, char *output, int output_maxncpy)
{
  MenuType *mt = UI_but_menutype_get(but);
  if (mt) {
    BLI_snprintf_utf8(output, output_maxncpy, "bpy.ops.wm.call_menu(name=\"%s\")", mt->idname);
    return true;
  }
  return false;
}

static bool ui_but_copy_popover(uiBut *but, char *output, int output_maxncpy)
{
  PanelType *pt = UI_but_paneltype_get(but);
  if (pt) {
    BLI_snprintf_utf8(output, output_maxncpy, "bpy.ops.wm.call_panel(name=\"%s\")", pt->idname);
    return true;
  }
  return false;
}

/** Returns true if any data was copied. */
static bool ui_but_copy(bContext *C, uiBut *but, const bool copy_array)
{
  if (ui_but_contains_password(but)) {
    return false;
  }

  /* Arbitrary large value (allow for paths: 'PATH_MAX') */
  char buf[4096] = {0};
  const int buf_maxncpy = sizeof(buf);

  /* Left false for copying internal data (color-band for eg). */
  bool is_buf_set = false;

  const bool has_required_data = !(but->poin == nullptr && but->rnapoin.data == nullptr);

  switch (but->type) {
    case ButType::Num:
    case ButType::NumSlider:
      if (!has_required_data) {
        break;
      }
      if (copy_array && ui_but_has_array_value(but)) {
        ui_but_copy_numeric_array(but, buf, buf_maxncpy);
      }
      else {
        ui_but_copy_numeric_value(but, buf, buf_maxncpy);
      }
      is_buf_set = true;
      break;

    case ButType::Unitvec:
      if (!has_required_data) {
        break;
      }
      ui_but_copy_numeric_array(but, buf, buf_maxncpy);
      is_buf_set = true;
      break;

    case ButType::Color:
      if (!has_required_data) {
        break;
      }
      ui_but_copy_color(but, buf, buf_maxncpy);
      is_buf_set = true;
      break;

    case ButType::Text:
    case ButType::SearchMenu:
      if (!has_required_data) {
        break;
      }
      ui_but_copy_text(but, buf, buf_maxncpy);
      is_buf_set = true;
      break;

    case ButType::ColorBand:
      ui_but_copy_colorband(but);
      break;

    case ButType::Curve:
      ui_but_copy_curvemapping(but);
      break;

    case ButType::CurveProfile:
      ui_but_copy_CurveProfile(but);
      break;

    case ButType::But:
      if (!but->optype) {
        break;
      }
      ui_but_copy_operator(C, but, buf, buf_maxncpy);
      is_buf_set = true;
      break;

    case ButType::Menu:
    case ButType::Pulldown:
      if (ui_but_copy_menu(but, buf, buf_maxncpy)) {
        is_buf_set = true;
      }
      break;
    case ButType::Popover:
      if (ui_but_copy_popover(but, buf, buf_maxncpy)) {
        is_buf_set = true;
      }
      break;

    default:
      break;
  }

  if (is_buf_set) {
    WM_clipboard_text_set(buf, false);
  }
  return is_buf_set;
}

static void ui_but_paste(bContext *C, uiBut *but, uiHandleButtonData *data, const bool paste_array)
{
  BLI_assert((but->flag & UI_BUT_DISABLED) == 0); /* caller should check */

  int buf_paste_len = 0;
  char *buf_paste;
  ui_but_get_pasted_text_from_clipboard(UI_but_is_utf8(but), &buf_paste, &buf_paste_len);

  const bool has_required_data = !(but->poin == nullptr && but->rnapoin.data == nullptr);

  switch (but->type) {
    case ButType::Num:
    case ButType::NumSlider:
      if (!has_required_data) {
        break;
      }
      if (paste_array && ui_but_has_array_value(but)) {
        ui_but_paste_numeric_array(C, but, data, buf_paste);
      }
      else {
        ui_but_paste_numeric_value(C, but, data, buf_paste);
      }
      break;

    case ButType::Unitvec:
      if (!has_required_data) {
        break;
      }
      ui_but_paste_normalized_vector(C, but, data, buf_paste);
      break;

    case ButType::Color:
      if (!has_required_data) {
        break;
      }
      ui_but_paste_color(C, but, buf_paste);
      break;

    case ButType::Text:
    case ButType::SearchMenu:
      if (!has_required_data) {
        break;
      }
      ui_but_paste_text(C, but, data, buf_paste);
      break;

    case ButType::ColorBand:
      ui_but_paste_colorband(C, but, data);
      break;

    case ButType::Curve:
      ui_but_paste_curvemapping(C, but);
      break;

    case ButType::CurveProfile:
      ui_but_paste_CurveProfile(C, but);
      break;

    default:
      break;
  }

  MEM_freeN(buf_paste);
}

void ui_but_clipboard_free()
{
  BKE_curvemapping_free_data(&but_copypaste_curve);
  BKE_curveprofile_free_data(&but_copypaste_profile);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Text Password
 *
 * Functions to convert password strings that should not be displayed
 * to asterisk representation (e.g. `mysecretpasswd` -> `*************`)
 *
 * It converts every UTF8 character to an asterisk, and also remaps
 * the cursor position and selection start/end.
 *
 * \note remapping is used, because password could contain UTF8 characters.
 *
 * \{ */

static int ui_text_position_from_hidden(uiBut *but, int pos)
{
  const char *butstr = (but->editstr) ? but->editstr : but->drawstr.c_str();
  const char *strpos = butstr;
  const char *str_end = butstr + strlen(butstr);
  for (int i = 0; i < pos; i++) {
    strpos = BLI_str_find_next_char_utf8(strpos, str_end);
  }

  return (strpos - butstr);
}

static int ui_text_position_to_hidden(uiBut *but, int pos)
{
  const char *butstr = (but->editstr) ? but->editstr : but->drawstr.c_str();
  return BLI_strnlen_utf8(butstr, pos);
}

void ui_but_text_password_hide(char password_str[UI_MAX_PASSWORD_STR],
                               uiBut *but,
                               const bool restore)
{
  if (!(but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_PASSWORD)) {
    return;
  }

  char *butstr = (but->editstr) ? but->editstr : but->drawstr.data();

  if (restore) {
    /* restore original string */
    BLI_strncpy(butstr, password_str, UI_MAX_PASSWORD_STR);

    /* remap cursor positions */
    if (but->pos >= 0) {
      but->pos = ui_text_position_from_hidden(but, but->pos);
      but->selsta = ui_text_position_from_hidden(but, but->selsta);
      but->selend = ui_text_position_from_hidden(but, but->selend);
    }
  }
  else {
    /* convert text to hidden text using asterisks (e.g. pass -> ****) */
    const size_t len = BLI_strlen_utf8(butstr);

    /* remap cursor positions */
    if (but->pos >= 0) {
      but->pos = ui_text_position_to_hidden(but, but->pos);
      but->selsta = ui_text_position_to_hidden(but, but->selsta);
      but->selend = ui_text_position_to_hidden(but, but->selend);
    }

    /* save original string */
    BLI_strncpy(password_str, butstr, UI_MAX_PASSWORD_STR);
    memset(butstr, '*', len);
    butstr[len] = '\0';
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Text Selection/Editing
 * \{ */

void ui_but_set_string_interactive(bContext *C, uiBut *but, const char *value)
{
  /* Caller should check. */
  BLI_assert((but->flag & UI_BUT_DISABLED) == 0);

  button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
  ui_textedit_string_set(but, but->active->text_edit, value);

  if (but->type == ButType::SearchMenu && but->active) {
    but->changed = true;
    ui_searchbox_update(C, but->active->searchbox, but, true);
  }

  button_activate_state(C, but, BUTTON_STATE_EXIT);
}

void ui_but_active_string_clear_and_exit(bContext *C, uiBut *but)
{
  if (!but->active) {
    return;
  }

  /* most likely nullptr, but let's check, and give it temp zero string */
  if (!but->active->text_edit.edit_string) {
    but->active->text_edit.edit_string = MEM_calloc_arrayN<char>(1, "temp str");
  }
  but->active->text_edit.edit_string[0] = 0;

  ui_apply_but_TEX(C, but, but->active);
  button_activate_state(C, but, BUTTON_STATE_EXIT);
}

static void ui_textedit_string_ensure_max_length(uiBut *but,
                                                 uiTextEdit &text_edit,
                                                 int str_maxncpy)
{
  BLI_assert(text_edit.is_str_dynamic);
  BLI_assert(text_edit.edit_string == but->editstr);

  if (str_maxncpy > text_edit.max_string_size) {
    text_edit.edit_string = but->editstr = static_cast<char *>(
        MEM_reallocN(text_edit.edit_string, sizeof(char) * str_maxncpy));
    text_edit.max_string_size = str_maxncpy;
  }
}

static void ui_textedit_string_set(uiBut *but, uiTextEdit &text_edit, const char *str)
{
  if (text_edit.is_str_dynamic) {
    ui_textedit_string_ensure_max_length(but, text_edit, strlen(str) + 1);
  }

  if (UI_but_is_utf8(but)) {
    BLI_strncpy_utf8(text_edit.edit_string, str, text_edit.max_string_size);
  }
  else {
    BLI_strncpy(text_edit.edit_string, str, text_edit.max_string_size);
  }
}

static bool ui_textedit_delete_selection(uiBut *but, uiTextEdit &text_edit)
{
  char *str = text_edit.edit_string;
  const int len = strlen(str);
  bool changed = false;
  if (but->selsta != but->selend && len) {
    memmove(str + but->selsta, str + but->selend, (len - but->selend) + 1);
    changed = true;
  }

  if (but->ofs > but->selsta) {
    /* Decrease the offset by the amount of the selection that is hidden. Without
     * this adjustment, pasting text that doesn't fit in the text field would leave
     * the pasted text scrolled out of the view (to the left), see: #134491. */
    but->ofs -= (but->ofs - but->selsta);
  }

  but->pos = but->selend = but->selsta;
  return changed;
}

/**
 * \param x: Screen space cursor location - #wmEvent.x
 *
 * \note `but->block->aspect` is used here, so drawing button style is getting scaled too.
 */
static void ui_textedit_set_cursor_pos(uiBut *but, const ARegion *region, const float x)
{
  /* XXX pass on as arg. */
  uiFontStyle fstyle = UI_style_get()->widget;
  const float aspect = but->block->aspect;

  float startx = but->rect.xmin;
  float starty_dummy = 0.0f;
  char password_str[UI_MAX_PASSWORD_STR];
  /* treat 'str_last' as null terminator for str, no need to modify in-place */
  const char *str = but->editstr, *str_last;

  ui_block_to_window_fl(region, but->block, &startx, &starty_dummy);

  ui_fontscale(&fstyle.points, aspect);

  UI_fontstyle_set(&fstyle);

  ui_but_text_password_hide(password_str, but, false);

  if (ELEM(but->type, ButType::Text, ButType::SearchMenu)) {
    if (but->flag & UI_HAS_ICON) {
      startx += UI_ICON_SIZE / aspect;
    }
  }
  startx -= U.pixelsize / aspect;
  if (!(but->drawflag & UI_BUT_NO_TEXT_PADDING)) {
    startx += UI_TEXT_MARGIN_X * U.widget_unit / aspect;
  }

  /* mouse dragged outside the widget to the left */
  if (x < startx) {
    int i = but->ofs;

    str_last = &str[but->ofs];

    while (i > 0) {
      if (BLI_str_cursor_step_prev_utf8(str, but->ofs, &i)) {
        /* 0.25 == scale factor for less sensitivity */
        if (BLF_width(fstyle.uifont_id, str + i, (str_last - str) - i) > (startx - x) * 0.25f) {
          break;
        }
      }
      else {
        break; /* unlikely but possible */
      }
    }
    but->ofs = i;
    but->pos = but->ofs;
  }
  /* mouse inside the widget, mouse coords mapped in widget space */
  else {
    but->pos = but->ofs +
               BLF_str_offset_from_cursor_position(
                   fstyle.uifont_id, str + but->ofs, strlen(str + but->ofs), int(x - startx));
  }

  ui_but_text_password_hide(password_str, but, true);
}

static void ui_textedit_set_cursor_select(uiBut *but, uiHandleButtonData *data, const float x)
{
  ui_textedit_set_cursor_pos(but, data->region, x);

  but->selsta = but->pos;
  but->selend = data->text_edit.sel_pos_init;
  if (but->selend < but->selsta) {
    std::swap(but->selsta, but->selend);
  }

  ui_but_update(but);
}

/**
 * This is used for both UTF8 and ASCII.
 *
 * For unicode buttons, \a buf is treated as unicode.
 */
static bool ui_textedit_insert_buf(uiBut *but, uiTextEdit &text_edit, const char *buf, int buf_len)
{
  int len = strlen(text_edit.edit_string);
  const int str_maxncpy_new = len - (but->selend - but->selsta) + 1;
  bool changed = false;

  if (text_edit.is_str_dynamic) {
    ui_textedit_string_ensure_max_length(but, text_edit, str_maxncpy_new + buf_len);
  }

  if (str_maxncpy_new <= text_edit.max_string_size) {
    char *str = text_edit.edit_string;
    size_t step = buf_len;

    /* type over the current selection */
    if ((but->selend - but->selsta) > 0) {
      changed = ui_textedit_delete_selection(but, text_edit);
      len = strlen(str);
    }

    if ((len + step >= text_edit.max_string_size) && (text_edit.max_string_size - (len + 1) > 0)) {
      if (UI_but_is_utf8(but)) {
        /* Shorten 'step' to a UTF8 aligned size that fits. */
        BLI_strnlen_utf8_ex(buf, text_edit.max_string_size - (len + 1), &step);
      }
      else {
        step = text_edit.max_string_size - (len + 1);
      }
    }

    if (step && (len + step < text_edit.max_string_size)) {
      memmove(&str[but->pos + step], &str[but->pos], (len + 1) - but->pos);
      memcpy(&str[but->pos], buf, step * sizeof(char));
      but->pos += step;
      changed = true;
    }
  }

  return changed;
}

#ifdef WITH_INPUT_IME
static bool ui_textedit_insert_ascii(uiBut *but, uiHandleButtonData *data, const char ascii)
{
  BLI_assert(isascii(ascii));
  const char buf[2] = {ascii, '\0'};
  return ui_textedit_insert_buf(but, data->text_edit, buf, sizeof(buf) - 1);
}
#endif

static void ui_textedit_move(uiBut *but,
                             uiTextEdit &text_edit,
                             eStrCursorJumpDirection direction,
                             const bool select,
                             eStrCursorJumpType jump)
{
  const char *str = text_edit.edit_string;
  const int len = strlen(str);
  const int pos_prev = but->pos;
  const bool has_sel = (but->selend - but->selsta) > 0;

  ui_but_update(but);

  /* special case, quit selection and set cursor */
  if (has_sel && !select) {
    if (jump == STRCUR_JUMP_ALL) {
      but->selsta = but->selend = but->pos = direction ? len : 0;
    }
    else {
      if (direction) {
        but->selsta = but->pos = but->selend;
      }
      else {
        but->pos = but->selend = but->selsta;
      }
    }
    text_edit.sel_pos_init = but->pos;
  }
  else {
    int pos_i = but->pos;
    BLI_str_cursor_step_utf8(str, len, &pos_i, direction, jump, true);
    but->pos = pos_i;

    if (select) {
      if (has_sel == false) {
        /* Holding shift but with no previous selection. */
        but->selsta = but->pos;
        but->selend = pos_prev;
      }
      else if (but->selsta == pos_prev) {
        /* Previous selection, extending start position. */
        but->selsta = but->pos;
      }
      else {
        /* Previous selection, extending end position. */
        but->selend = but->pos;
      }
    }
    if (but->selend < but->selsta) {
      std::swap(but->selsta, but->selend);
    }
  }
}

static bool ui_textedit_delete(uiBut *but,
                               uiTextEdit &text_edit,
                               eStrCursorJumpDirection direction,
                               eStrCursorJumpType jump)
{
  char *str = text_edit.edit_string;
  const int len = strlen(str);

  bool changed = false;

  if (jump == STRCUR_JUMP_ALL) {
    if (len) {
      changed = true;
    }
    str[0] = '\0';
    but->pos = 0;
  }
  else if (direction) { /* delete */
    if ((but->selend - but->selsta) > 0) {
      changed = ui_textedit_delete_selection(but, text_edit);
    }
    else if (but->pos >= 0 && but->pos < len) {
      int pos = but->pos;
      int step;
      BLI_str_cursor_step_utf8(str, len, &pos, direction, jump, true);
      step = pos - but->pos;
      memmove(&str[but->pos], &str[but->pos + step], (len + 1) - (but->pos + step));
      changed = true;
    }
  }
  else { /* backspace */
    if (len != 0) {
      if ((but->selend - but->selsta) > 0) {
        changed = ui_textedit_delete_selection(but, text_edit);
      }
      else if (but->pos > 0) {
        int pos = but->pos;
        int step;

        BLI_str_cursor_step_utf8(str, len, &pos, direction, jump, true);
        step = but->pos - pos;
        memmove(&str[but->pos - step], &str[but->pos], (len + 1) - but->pos);
        but->pos -= step;
        changed = true;
      }
    }
  }

  return changed;
}

static int ui_textedit_autocomplete(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  char *str = data->text_edit.edit_string;

  int changed;
  if (data->searchbox) {
    changed = ui_searchbox_autocomplete(C, data->searchbox, but, data->text_edit.edit_string);
  }
  else {
    changed = but->autocomplete_func(C, str, but->autofunc_arg);
  }

  but->pos = strlen(str);
  but->selsta = but->selend = but->pos;

  return changed;
}

/* mode for ui_textedit_copypaste() */
enum {
  UI_TEXTEDIT_PASTE = 1,
  UI_TEXTEDIT_COPY,
  UI_TEXTEDIT_CUT,
};

static bool ui_textedit_copypaste(uiBut *but, uiTextEdit &text_edit, const int mode)
{
  bool changed = false;

  /* paste */
  if (mode == UI_TEXTEDIT_PASTE) {
    /* extract the first line from the clipboard */
    int buf_len;
    char *pbuf = WM_clipboard_text_get_firstline(false, UI_but_is_utf8(but), &buf_len);

    if (pbuf) {
      ui_textedit_insert_buf(but, text_edit, pbuf, buf_len);

      changed = true;

      MEM_freeN(pbuf);
    }
  }
  /* cut & copy */
  else if (ELEM(mode, UI_TEXTEDIT_COPY, UI_TEXTEDIT_CUT)) {
    /* copy the contents to the copypaste buffer */
    const int sellen = but->selend - but->selsta;
    char *buf = MEM_malloc_arrayN<char>((sellen + 1), "ui_textedit_copypaste");

    memcpy(buf, text_edit.edit_string + but->selsta, sellen);
    buf[sellen] = '\0';

    WM_clipboard_text_set(buf, false);
    MEM_freeN(buf);

    /* for cut only, delete the selection afterwards */
    if (mode == UI_TEXTEDIT_CUT) {
      if ((but->selend - but->selsta) > 0) {
        changed = ui_textedit_delete_selection(but, text_edit);
      }
    }
  }

  return changed;
}

#ifdef WITH_INPUT_IME
/* Enable IME, and setup #uiBut IME data. */
static void ui_textedit_ime_begin(wmWindow *win, uiBut * /*but*/)
{
  /* XXX Is this really needed? */
  int x, y;

  BLI_assert(win->runtime->ime_data == nullptr);

  /* enable IME and position to cursor, it's a trick */
  x = win->eventstate->xy[0];
  /* flip y and move down a bit, prevent the IME panel cover the edit button */
  y = win->eventstate->xy[1] - 12;

  wm_window_IME_begin(win, x, y, 0, 0, true);
}

/* Disable IME, and clear #uiBut IME data. */
static void ui_textedit_ime_end(wmWindow *win, uiBut * /*but*/)
{
  wm_window_IME_end(win);
}

void ui_but_ime_reposition(uiBut *but, int x, int y, bool complete)
{
  BLI_assert(but->active || but->semi_modal_state);
  uiHandleButtonData *data = but->semi_modal_state ? but->semi_modal_state : but->active;

  ui_region_to_window(data->region, &x, &y);
  wm_window_IME_begin(data->window, x, y - 4, 0, 0, complete);
}

const wmIMEData *ui_but_ime_data_get(uiBut *but)
{
  uiHandleButtonData *data = but->semi_modal_state ? but->semi_modal_state : but->active;

  if (data && data->window && data->window->runtime->ime_data_is_composing) {
    return data->window->runtime->ime_data;
  }
  return nullptr;
}
#endif /* WITH_INPUT_IME */

static void ui_textedit_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  uiTextEdit &text_edit = data->text_edit;
  wmWindow *win = data->window;
  const bool is_num_but = ELEM(but->type, ButType::Num, ButType::NumSlider);
  bool no_zero_strip = false;

  MEM_SAFE_FREE(text_edit.edit_string);

  WorkspaceStatus status(C);

#if defined(__APPLE__)
  const int ctrl_icon = ICON_KEY_COMMAND;
#else
  const int ctrl_icon = ICON_EVENT_CTRL;
#endif

  status.item(IFACE_("Confirm"), ICON_EVENT_RETURN);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC);

  if (!is_num_but) {
    status.item(IFACE_("Select All"), ctrl_icon, ICON_EVENT_A);
    status.item(IFACE_("Copy"), ctrl_icon, ICON_EVENT_C);
    status.item(IFACE_("Paste"), ctrl_icon, ICON_EVENT_V);
  }

  if (but->autocomplete_func || data->searchbox) {
    status.item(IFACE_("Autocomplete"), ICON_EVENT_TAB);
  }

#ifdef USE_DRAG_MULTINUM
  /* this can happen from multi-drag */
  if (data->applied_interactive) {
    /* remove any small changes so canceling edit doesn't restore invalid value: #40538 */
    data->cancel = true;
    ui_apply_but(C, but->block, but, data, true);
    data->cancel = false;

    data->applied_interactive = false;
  }
#endif

#ifdef USE_ALLSELECT
  if (is_num_but) {
    if (IS_ALLSELECT_EVENT(win->eventstate)) {
      data->select_others.is_enabled = true;
      data->select_others.is_copy = true;
    }
  }
#endif

  /* retrieve string */
  text_edit.max_string_size = ui_but_string_get_maxncpy(but);
  if (text_edit.max_string_size != 0) {
    text_edit.edit_string = MEM_calloc_arrayN<char>(text_edit.max_string_size, "textedit str");
    /* We do not want to truncate precision to default here, it's nice to show value,
     * not to edit it - way too much precision is lost then. */
    ui_but_string_get_ex(but,
                         text_edit.edit_string,
                         text_edit.max_string_size,
                         UI_PRECISION_FLOAT_MAX,
                         true,
                         &no_zero_strip);
  }
  else {
    text_edit.is_str_dynamic = true;
    text_edit.edit_string = ui_but_string_get_dynamic(but, &text_edit.max_string_size);
  }

  if (ui_but_is_float(but) && !ui_but_is_unit(but) &&
      !ui_but_anim_expression_get(but, nullptr, 0) && !no_zero_strip)
  {
    BLI_str_rstrip_float_zero(text_edit.edit_string, '\0');
  }

  if (is_num_but) {
    BLI_assert(text_edit.is_str_dynamic == false);
    ui_but_convert_to_unit_alt_name(but, text_edit.edit_string, text_edit.max_string_size);

    ui_numedit_begin_set_values(but, data);
  }

  /* won't change from now on */
  const int len = strlen(text_edit.edit_string);

  text_edit.original_string = BLI_strdupn(text_edit.edit_string, len);
  text_edit.sel_pos_init = 0;

  /* set cursor pos to the end of the text */
  but->editstr = text_edit.edit_string;
  but->pos = len;
  if (bool(but->flag2 & UI_BUT2_ACTIVATE_ON_INIT_NO_SELECT)) {
    but->selsta = len;
  }
  else {
    but->selsta = 0;
  }
  but->selend = len;

  /* Initialize undo history tracking. */
  text_edit.undo_stack_text = ui_textedit_undo_stack_create();
  ui_textedit_undo_push(text_edit.undo_stack_text, but->editstr, but->pos);

  /* optional searchbox */
  if (but->type == ButType::SearchMenu) {
    uiButSearch *search_but = (uiButSearch *)but;

    data->searchbox = search_but->popup_create_fn(C, data->region, search_but);
    ui_searchbox_update(C, data->searchbox, but, true); /* true = reset */
  }

  /* reset alert flag (avoid confusion, will refresh on exit) */
  but->flag &= ~UI_BUT_REDALERT;

  ui_but_update(but);

  /* Make sure the edited button is in view. */
  if (data->searchbox) {
    /* Popup blocks don't support moving after creation, so don't change the view for them. */
  }
  else if (blender::ui::block_layout_needs_resolving(but->block)) {
    /* Layout isn't resolved yet (may happen when activating while drawing through
     * #UI_but_active_only()), so can't move it into view yet. This causes
     * #ui_but_update_view_for_active() to run after the layout is resolved. */
    but->changed = true;
  }
  else if ((but->block->flag & UI_BLOCK_CLIP_EVENTS) == 0) {
    /* Blocks with UI_BLOCK_CLIP_EVENTS are overlapping their region, so scrolling
     * that region to ensure it is in view can't work and causes issues. #97530 */
    UI_but_ensure_in_view(C, data->region, but);
  }

  WM_cursor_modal_set(win, WM_CURSOR_TEXT_EDIT);

  /* Temporarily turn off window auto-focus on platforms that support it. */
  GHOST_SetAutoFocus(false);

#ifdef WITH_INPUT_IME
  if (!is_num_but) {
    ui_textedit_ime_begin(win, but);
  }
#endif
}

static void ui_textedit_end(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  uiTextEdit &text_edit = data->text_edit;
  wmWindow *win = data->window;

  ED_workspace_status_text(C, nullptr);

  if (but) {
    if (UI_but_is_utf8(but)) {
      const int strip = BLI_str_utf8_invalid_strip(but->editstr, strlen(but->editstr));
      /* Strip non-UTF8 characters unless buttons support this.
       * This should never happen as all text input should be valid UTF8,
       * there is a small chance existing data contains invalid sequences.
       * This could check could be made into an assertion if `but->editstr`
       * is valid UTF8 when #ui_textedit_begin assigns the string. */
      if (strip) {
        CLOG_INFO_NOCHECK(&LOG, "%s: invalid utf8 - stripped chars %d", __func__, strip);
      }
    }

    if (data->searchbox) {
      if (data->cancel == false) {
        BLI_assert(but->type == ButType::SearchMenu);
        uiButSearch *but_search = (uiButSearch *)but;

        if ((ui_searchbox_apply(but, data->searchbox) == false) &&
            (ui_searchbox_find_index(data->searchbox, but->editstr) == -1) &&
            !but_search->results_are_suggestions)
        {

          if (but->flag & UI_BUT_VALUE_CLEAR) {
            /* It is valid for _VALUE_CLEAR flavor to have no active element
             * (it's a valid way to unlink). */
            but->editstr[0] = '\0';
          }
          data->cancel = true;

          /* ensure menu (popup) too is closed! */
          data->escapecancel = true;

          WM_global_reportf(RPT_ERROR, "Failed to find '%s'", but->editstr);
          WM_report_banner_show(CTX_wm_manager(C), win);
        }
      }

      ui_searchbox_free(C, data->searchbox);
      data->searchbox = nullptr;
    }

    but->editstr = nullptr;
    but->pos = -1;
  }

  WM_cursor_modal_restore(win);

  /* Turn back on the auto-focusing of windows. */
  GHOST_SetAutoFocus(true);

  /* Free text undo history text blocks. */
  ui_textedit_undo_stack_destroy(text_edit.undo_stack_text);
  text_edit.undo_stack_text = nullptr;

#ifdef WITH_INPUT_IME
  /* See #wm_window_IME_end code-comments for details. */
#  ifdef __APPLE__
  if (win->runtime->ime_data)
#  endif
  {
    ui_textedit_ime_end(win, but);
  }
#endif
}

static void ui_textedit_next_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
  /* Label and round-box can overlap real buttons (backdrops...). */
  if (ELEM(actbut->type,
           ButType::Label,
           ButType::Sepr,
           ButType::SeprLine,
           ButType::Roundbox,
           ButType::ListBox))
  {
    return;
  }

  for (int64_t i = block->but_index(actbut) + 1; i < block->buttons.size(); i++) {
    uiBut *but = block->buttons[i].get();
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & (UI_BUT_DISABLED | UI_HIDDEN))) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
  for (const std::unique_ptr<uiBut> &but : block->buttons) {
    if (but.get() == actbut) {
      break;
    }
    if (ui_but_is_editable_as_text(but.get())) {
      if (!(but->flag & (UI_BUT_DISABLED | UI_HIDDEN))) {
        data->postbut = but.get();
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
}

static void ui_textedit_prev_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
  /* Label and round-box can overlap real buttons (backdrops...). */
  if (ELEM(actbut->type,
           ButType::Label,
           ButType::Sepr,
           ButType::SeprLine,
           ButType::Roundbox,
           ButType::ListBox))
  {
    return;
  }

  for (int i = block->but_index(actbut) - 1; i >= 0; i--) {
    uiBut *but = block->buttons[i].get();
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & (UI_BUT_DISABLED | UI_HIDDEN))) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
  for (int i = block->buttons.size() - 1; i >= 0; i--) {
    uiBut *but = block->buttons[i].get();
    if (but == actbut) {
      break;
    }
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & (UI_BUT_DISABLED | UI_HIDDEN))) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
}

/**
 * Return the jump type used for cursor motion & back-space/delete actions.
 */
static eStrCursorJumpType ui_textedit_jump_type_from_event(const wmEvent *event)
{
#if defined(__APPLE__)
  if (event->modifier & KM_OSKEY) {
    return STRCUR_JUMP_ALL;
  }
  if (event->modifier & KM_ALT) {
    return STRCUR_JUMP_DELIM;
  }
#else
  if (event->modifier & KM_CTRL) {
    return STRCUR_JUMP_DELIM;
  }
#endif
  return STRCUR_JUMP_NONE;
}

static int ui_do_but_textedit(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  uiTextEdit &text_edit = data->text_edit;
  int retval = WM_UI_HANDLER_CONTINUE;
  bool changed = false, inbox = false, update = false, skip_undo_push = false;

#ifdef WITH_INPUT_IME
  wmWindow *win = CTX_wm_window(C);
  const wmIMEData *ime_data = win->runtime->ime_data;
  const bool is_ime_composing = ime_data && win->runtime->ime_data_is_composing;
#else
  const bool is_ime_composing = false;
#endif

  switch (event->type) {
    case MOUSEMOVE:
    case MOUSEPAN:
      if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
        if ((event->type == MOUSEMOVE) &&
            ui_mouse_motion_keynav_test(&data->searchbox_keynav_state, event))
        {
          /* pass */
        }
        else {
          ui_searchbox_event(C, data->searchbox, but, data->region, event);
        }
#else
        ui_searchbox_event(C, data->searchbox, but, data->region, event);
#endif
      }
      ui_do_but_extra_operator_icons_mousemove(but, data, event);

      break;
    case RIGHTMOUSE:
    case EVT_ESCKEY:
      /* Don't consume cancel events (would usually end text editing), let menu code handle it. */
      if (data->is_semi_modal) {
        break;
      }
      if (event->val == KM_PRESS) {
        /* Support search context menu. */
        if (event->type == RIGHTMOUSE) {
          if (data->searchbox) {
            if (ui_searchbox_event(C, data->searchbox, but, data->region, event)) {
              /* Only break if the event was handled. */
              break;
            }
          }
        }

#ifdef WITH_INPUT_IME
        /* skips button handling since it is not wanted */
        if (is_ime_composing) {
          break;
        }
#endif
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
      }
      break;
    case LEFTMOUSE: {
      /* Allow clicks on extra icons while editing. */
      if (ui_do_but_extra_operator_icon(C, but, data, event)) {
        break;
      }

      const bool had_selection = but->selsta != but->selend;

      /* exit on LMB only on RELEASE for searchbox, to mimic other popups,
       * and allow multiple menu levels */
      if (data->searchbox) {
        inbox = ui_searchbox_inside(data->searchbox, event->xy);
      }

      bool is_press_in_button = false;
      if (ELEM(event->val, KM_PRESS, KM_DBL_CLICK)) {
        float mx = event->xy[0];
        float my = event->xy[1];
        ui_window_to_block_fl(data->region, block, &mx, &my);

        if (ui_but_contains_pt(but, mx, my)) {
          is_press_in_button = true;
        }
      }

      /* for double click: we do a press again for when you first click on button
       * (selects all text, no cursor pos) */
      if (ELEM(event->val, KM_PRESS, KM_DBL_CLICK)) {
        if (is_press_in_button) {
          ui_textedit_set_cursor_pos(but, data->region, event->xy[0]);
          but->selsta = but->selend = but->pos;
          text_edit.sel_pos_init = but->pos;

          button_activate_state(C, but, BUTTON_STATE_TEXT_SELECTING);
          retval = WM_UI_HANDLER_BREAK;
        }
        else if (inbox == false && !data->is_semi_modal) {
          /* if searchbox, click outside will cancel */
          if (data->searchbox) {
            data->cancel = data->escapecancel = true;
          }
          button_activate_state(C, but, BUTTON_STATE_EXIT);
          retval = WM_UI_HANDLER_BREAK;
        }
      }

      /* only select a word in button if there was no selection before */
      if (event->val == KM_DBL_CLICK && had_selection == false) {
        if (is_press_in_button) {
          const int str_len = strlen(text_edit.edit_string);
          /* This may not be necessary, additional check to ensure `pos` is never out of range,
           * since negative values aren't acceptable, see: #113154. */
          CLAMP(but->pos, 0, str_len);

          int selsta, selend;
          BLI_str_cursor_step_bounds_utf8(
              text_edit.edit_string, str_len, but->pos, &selsta, &selend);
          but->pos = short(selend);
          but->selsta = short(selsta);
          but->selend = short(selend);
          /* Anchor selection to the left side unless the last word. */
          text_edit.sel_pos_init = ((selend == str_len) && (selsta != 0)) ? selend : selsta;
          retval = WM_UI_HANDLER_BREAK;
          changed = true;
        }
      }
      else if (inbox && !data->is_semi_modal) {
        /* if we allow activation on key press,
         * it gives problems launching operators #35713. */
        if (event->val == KM_RELEASE) {
          button_activate_state(C, but, BUTTON_STATE_EXIT);
          retval = WM_UI_HANDLER_BREAK;
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  if (event->val == KM_PRESS && !is_ime_composing) {
    switch (event->type) {
      case EVT_VKEY:
      case EVT_XKEY:
      case EVT_CKEY:
#if defined(__APPLE__)
        if (ELEM(event->modifier, KM_OSKEY, KM_CTRL))
#else
        if (event->modifier == KM_CTRL)
#endif
        {
          if (event->type == EVT_VKEY) {
            changed = ui_textedit_copypaste(but, text_edit, UI_TEXTEDIT_PASTE);
          }
          else if (event->type == EVT_CKEY) {
            changed = ui_textedit_copypaste(but, text_edit, UI_TEXTEDIT_COPY);
          }
          else if (event->type == EVT_XKEY) {
            changed = ui_textedit_copypaste(but, text_edit, UI_TEXTEDIT_CUT);
          }

          retval = WM_UI_HANDLER_BREAK;
        }
        break;
      case EVT_RIGHTARROWKEY:
      case EVT_LEFTARROWKEY: {
        const eStrCursorJumpDirection direction = (event->type == EVT_RIGHTARROWKEY) ?
                                                      STRCUR_DIR_NEXT :
                                                      STRCUR_DIR_PREV;
        const eStrCursorJumpType jump = ui_textedit_jump_type_from_event(event);
        ui_textedit_move(but, text_edit, direction, event->modifier & KM_SHIFT, jump);
        retval = WM_UI_HANDLER_BREAK;
        break;
      }
      case WHEELDOWNMOUSE:
      case EVT_DOWNARROWKEY:
        if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
          ui_mouse_motion_keynav_init(&data->searchbox_keynav_state, event);
#endif
          ui_searchbox_event(C, data->searchbox, but, data->region, event);
          break;
        }
        if (event->type == WHEELDOWNMOUSE) {
          break;
        }
        ATTR_FALLTHROUGH;
      case EVT_ENDKEY:
        ui_textedit_move(
            but, text_edit, STRCUR_DIR_NEXT, event->modifier & KM_SHIFT, STRCUR_JUMP_ALL);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case WHEELUPMOUSE:
      case EVT_UPARROWKEY:
        if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
          ui_mouse_motion_keynav_init(&data->searchbox_keynav_state, event);
#endif
          ui_searchbox_event(C, data->searchbox, but, data->region, event);
          break;
        }
        if (event->type == WHEELUPMOUSE) {
          break;
        }
        ATTR_FALLTHROUGH;
      case EVT_HOMEKEY:
        ui_textedit_move(
            but, text_edit, STRCUR_DIR_PREV, event->modifier & KM_SHIFT, STRCUR_JUMP_ALL);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case EVT_PADENTER:
      case EVT_RETKEY:
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case EVT_DELKEY:
      case EVT_BACKSPACEKEY: {
        const eStrCursorJumpDirection direction = (event->type == EVT_DELKEY) ? STRCUR_DIR_NEXT :
                                                                                STRCUR_DIR_PREV;
        const eStrCursorJumpType jump = ui_textedit_jump_type_from_event(event);
        changed = ui_textedit_delete(but, text_edit, direction, jump);
        retval = WM_UI_HANDLER_BREAK;
        break;
      }

      case EVT_AKEY:

        /* Ctrl-A: Select all. */
#if defined(__APPLE__)
        /* OSX uses Command-A system-wide, so add it. */
        if (ELEM(event->modifier, KM_OSKEY, KM_CTRL))
#else
        if (event->modifier == KM_CTRL)
#endif
        {
          ui_textedit_move(but, text_edit, STRCUR_DIR_PREV, false, STRCUR_JUMP_ALL);
          ui_textedit_move(but, text_edit, STRCUR_DIR_NEXT, true, STRCUR_JUMP_ALL);
          retval = WM_UI_HANDLER_BREAK;
        }
        break;

      case EVT_TABKEY:
        /* There is a key conflict here, we can't tab with auto-complete. */
        if (but->autocomplete_func || data->searchbox) {
          const int autocomplete = ui_textedit_autocomplete(C, but, data);
          changed = autocomplete != AUTOCOMPLETE_NO_MATCH;

          if (autocomplete == AUTOCOMPLETE_FULL_MATCH) {
            button_activate_state(C, but, BUTTON_STATE_EXIT);
          }
        }
        else if ((event->modifier & ~KM_SHIFT) == 0) {
          /* Use standard keys for cycling through buttons Tab, Shift-Tab to reverse. */
          if (event->modifier & KM_SHIFT) {
            ui_textedit_prev_but(block, but, data);
          }
          else {
            ui_textedit_next_but(block, but, data);
          }
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        retval = WM_UI_HANDLER_BREAK;
        break;
      case EVT_ZKEY: {
        /* Ctrl-Z or Ctrl-Shift-Z: Undo/Redo (allowing for OS-Key on Apple). */

        const bool is_redo = (event->modifier & KM_SHIFT);
        if (
#if defined(__APPLE__)
            ((event->modifier & KM_OSKEY) && ((event->modifier & (KM_ALT | KM_CTRL)) == 0)) ||
#endif
            ((event->modifier & KM_CTRL) && ((event->modifier & (KM_ALT | KM_OSKEY)) == 0)))
        {
          int undo_pos;
          const char *undo_str = ui_textedit_undo(
              text_edit.undo_stack_text, is_redo ? 1 : -1, &undo_pos);
          if (undo_str != nullptr) {
            ui_textedit_string_set(but, text_edit, undo_str);

            /* Set the cursor & clear selection. */
            but->pos = undo_pos;
            but->selsta = but->pos;
            but->selend = but->pos;
            changed = true;
          }
          retval = WM_UI_HANDLER_BREAK;
          skip_undo_push = true;
        }
        break;
      }
      default: {
        break;
      }
    }

    if ((event->utf8_buf[0]) && (retval == WM_UI_HANDLER_CONTINUE)
#ifdef WITH_INPUT_IME
        && !is_ime_composing && !WM_event_is_ime_switch(event)
#endif
    )
    {
      char utf8_buf_override[2] = {'\0', '\0'};
      const char *utf8_buf = event->utf8_buf;

      /* Exception that's useful for number buttons, some keyboard
       * numpads have a comma instead of a period. */
      if (ELEM(but->type, ButType::Num, ButType::NumSlider)) { /* Could use `data->min`. */
        if ((event->type == EVT_PADPERIOD) && (utf8_buf[0] == ',')) {
          utf8_buf_override[0] = '.';
          utf8_buf = utf8_buf_override;
        }
      }

      if (utf8_buf[0]) {
        const int utf8_buf_len = BLI_str_utf8_size_or_error(utf8_buf);
        BLI_assert(utf8_buf_len != -1);
        changed = ui_textedit_insert_buf(but, text_edit, utf8_buf, utf8_buf_len);
      }

      retval = WM_UI_HANDLER_BREAK;
    }
    /* textbutton with this flag: do live update (e.g. for search buttons) */
    if (but->flag & UI_BUT_TEXTEDIT_UPDATE) {
      update = true;
    }
  }

#ifdef WITH_INPUT_IME
  if (event->type == WM_IME_COMPOSITE_START) {
    changed = true;
    if (but->selend > but->selsta) {
      ui_textedit_delete_selection(but, text_edit);
    }
  }
  else if (event->type == WM_IME_COMPOSITE_EVENT) {
    changed = true;
    if (ime_data->result.size()) {
      if (ELEM(but->type, ButType::Num, ButType::NumSlider) &&
          STREQ(ime_data->result.c_str(), "\xE3\x80\x82"))
      {
        /* Convert Ideographic Full Stop (U+3002) to decimal point when entering numbers. */
        ui_textedit_insert_ascii(but, data, '.');
      }
      else {
        ui_textedit_insert_buf(but, text_edit, ime_data->result.c_str(), ime_data->result.size());
      }
    }
  }
  else if (event->type == WM_IME_COMPOSITE_END) {
    changed = true;
  }
#endif

  if (changed) {
    /* The undo stack may be nullptr if an event exits editing. */
    if ((skip_undo_push == false) && (text_edit.undo_stack_text != nullptr)) {
      ui_textedit_undo_push(text_edit.undo_stack_text, text_edit.edit_string, but->pos);
    }

    /* only do live update when but flag request it (UI_BUT_TEXTEDIT_UPDATE). */
    if (update && data->interactive) {
      ui_apply_but(C, block, but, data, true);
    }
    else {
      ui_but_update_edited(but);
    }
    but->changed = true;

    if (data->searchbox) {
      ui_searchbox_update(C, data->searchbox, but, true); /* true = reset */
    }
  }

  if (changed || (retval == WM_UI_HANDLER_BREAK)) {
    ED_region_tag_redraw(data->region);
    if (!data->searchbox) {
      /* In case of popup regions, tag for popup refreshing too (contents may have changed). Not
       * done for search-boxes, since they have their own update handling. */
      ED_region_tag_refresh_ui(data->region);
    }
  }

  /* Swallow all events unless semi-modal handling is requested. */
  return data->is_semi_modal ? retval : WM_UI_HANDLER_BREAK;
}

static int ui_do_but_textedit_select(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int retval = WM_UI_HANDLER_CONTINUE;

  switch (event->type) {
    case MOUSEMOVE: {
      int mx = event->xy[0];
      int my = event->xy[1];
      ui_window_to_block(data->region, block, &mx, &my);

      ui_textedit_set_cursor_select(but, data, event->xy[0]);
      retval = WM_UI_HANDLER_BREAK;
      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      }
      retval = WM_UI_HANDLER_BREAK;
      break;
    default: {
      break;
    }
  }

  if (retval == WM_UI_HANDLER_BREAK) {
    ui_but_update(but);
    ED_region_tag_redraw(data->region);
  }

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Number Editing (various types)
 * \{ */

static void ui_numedit_begin_set_values(uiBut *but, uiHandleButtonData *data)
{
  data->startvalue = ui_but_value_get(but);
  data->origvalue = data->startvalue;
  data->value = data->origvalue;
}

static void ui_numedit_begin(uiBut *but, uiHandleButtonData *data)
{
  if (but->type == ButType::Curve) {
    uiButCurveMapping *but_cumap = (uiButCurveMapping *)but;
    but_cumap->edit_cumap = (CurveMapping *)but->poin;
  }
  else if (but->type == ButType::CurveProfile) {
    uiButCurveProfile *but_profile = (uiButCurveProfile *)but;
    but_profile->edit_profile = (CurveProfile *)but->poin;
  }
  else if (but->type == ButType::ColorBand) {
    uiButColorBand *but_coba = (uiButColorBand *)but;
    data->coba = (ColorBand *)but->poin;
    but_coba->edit_coba = data->coba;
  }
  else if (ELEM(but->type, ButType::Unitvec, ButType::HsvCube, ButType::HsvCircle, ButType::Color))
  {
    ui_but_v3_get(but, data->origvec);
    copy_v3_v3(data->vec, data->origvec);
    but->editvec = data->vec;
  }
  else {
    ui_numedit_begin_set_values(but, data);
    but->editval = &data->value;

    float softmin = but->softmin;
    float softmax = but->softmax;
    float softrange = softmax - softmin;
    const PropertyScaleType scale_type = ui_but_scale_type(but);

    float log_min = (scale_type == PROP_SCALE_LOG) ? max_ff(softmin, UI_PROP_SCALE_LOG_MIN) : 0.0f;

    if ((but->type == ButType::Num) && (ui_but_is_cursor_warp(but) == false)) {
      uiButNumber *number_but = (uiButNumber *)but;

      if (scale_type == PROP_SCALE_LOG) {
        log_min = max_ff(log_min, powf(10, -number_but->precision) * 0.5f);
      }
      /* Use a minimum so we have a predictable range,
       * otherwise some float buttons get a large range. */
      const float value_step_float_min = 0.1f;
      const bool is_float = ui_but_is_float(but);
      const double value_step = is_float ?
                                    double(number_but->step_size * UI_PRECISION_FLOAT_SCALE) :
                                    int(number_but->step_size);
      const float drag_map_softrange_max = UI_DRAG_MAP_SOFT_RANGE_PIXEL_MAX * UI_SCALE_FAC;
      const float softrange_max = min_ff(
          softrange,
          2 * (is_float ? min_ff(value_step, value_step_float_min) *
                              (drag_map_softrange_max / value_step_float_min) :
                          drag_map_softrange_max));

      if (softrange > softrange_max) {
        /* Center around the value, keeping in the real soft min/max range. */
        softmin = data->origvalue - (softrange_max / 2);
        softmax = data->origvalue + (softrange_max / 2);
        if (!isfinite(softmin)) {
          softmin = (data->origvalue > 0.0f ? FLT_MAX : -FLT_MAX);
        }
        if (!isfinite(softmax)) {
          softmax = (data->origvalue > 0.0f ? FLT_MAX : -FLT_MAX);
        }

        if (softmin < but->softmin) {
          softmin = but->softmin;
          softmax = softmin + softrange_max;
        }
        else if (softmax > but->softmax) {
          softmax = but->softmax;
          softmin = softmax - softrange_max;
        }

        /* Can happen at extreme values. */
        if (UNLIKELY(softmin == softmax)) {
          if (data->origvalue > 0.0) {
            softmin = nextafterf(softmin, -FLT_MAX);
          }
          else {
            softmax = nextafterf(softmax, FLT_MAX);
          }
        }

        softrange = softmax - softmin;
      }
    }

    if (softrange == 0.0f) {
      data->dragfstart = 0.0f;
    }
    else {
      switch (scale_type) {
        case PROP_SCALE_LINEAR: {
          data->dragfstart = (float(data->value) - softmin) / softrange;
          break;
        }
        case PROP_SCALE_LOG: {
          BLI_assert(log_min != 0.0f);
          const float base = softmax / log_min;
          data->dragfstart = logf(float(data->value) / log_min) / logf(base);
          break;
        }
        case PROP_SCALE_CUBIC: {
          const float cubic_min = cube_f(softmin);
          const float cubic_max = cube_f(softmax);
          const float cubic_range = cubic_max - cubic_min;
          const float f = (float(data->value) - softmin) * cubic_range / softrange + cubic_min;
          data->dragfstart = (cbrtf(f) - softmin) / softrange;
          break;
        }
      }
    }
    data->dragf = data->dragfstart;

    data->drag_map_soft_min = softmin;
    data->drag_map_soft_max = softmax;
  }

  data->dragchange = false;
  data->draglock = true;
}

static void ui_numedit_end(uiBut *but, uiHandleButtonData *data)
{
  but->editval = nullptr;
  but->editvec = nullptr;
  if (but->type == ButType::ColorBand) {
    uiButColorBand *but_coba = (uiButColorBand *)but;
    but_coba->edit_coba = nullptr;
  }
  else if (but->type == ButType::Curve) {
    uiButCurveMapping *but_cumap = (uiButCurveMapping *)but;
    but_cumap->edit_cumap = nullptr;
  }
  else if (but->type == ButType::CurveProfile) {
    uiButCurveProfile *but_profile = (uiButCurveProfile *)but;
    but_profile->edit_profile = nullptr;
  }
  data->dragstartx = 0;
  data->draglastx = 0;
  data->dragchange = false;
  data->dragcbd = nullptr;
  data->dragsel = 0;
}

static void ui_numedit_apply(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
  if (data->interactive) {
    ui_apply_but(C, block, but, data, true);
  }
  else {
    ui_but_update(but);
  }

  ED_region_tag_redraw(data->region);
}

static void ui_but_extra_operator_icon_apply(bContext *C, uiBut *but, uiButExtraOpIcon *op_icon)
{
  but->active->apply_through_extra_icon = true;

  if (but->active->interactive) {
    ui_apply_but(C, but->block, but, but->active, true);
  }
  button_activate_state(C, but, BUTTON_STATE_EXIT);
  WM_operator_name_call_ptr_with_depends_on_cursor(C,
                                                   op_icon->optype_params->optype,
                                                   op_icon->optype_params->opcontext,
                                                   op_icon->optype_params->opptr,
                                                   nullptr,
                                                   "");

  /* Force recreation of extra operator icons (pseudo update). */
  ui_but_extra_operator_icons_free(but);

  WM_event_add_mousemove(CTX_wm_window(C));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu/Popup Begin/End (various popup types)
 * \{ */

static void ui_block_open_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  uiBlockCreateFunc func = nullptr;
  uiBlockHandleCreateFunc handlefunc = nullptr;
  uiMenuCreateFunc menufunc = nullptr;
  uiMenuCreateFunc popoverfunc = nullptr;
  /* The checks for the panel type being null are for exceptional cases where script
   * authors intentionally unregister built-in panels for example.
   * While this should only ever happen rarely, it shouldn't crash, see #144716. */
  PanelType *popover_panel_type = nullptr;
  void *arg = nullptr;

  if (but->type != ButType::Pulldown) {
    /* Clear the status bar. */
    WorkspaceStatus status(C);
    status.item(" ", ICON_NONE);
  }

  switch (but->type) {
    case ButType::Block:
    case ButType::Pulldown:
      if (but->menu_create_func) {
        menufunc = but->menu_create_func;
        arg = but->poin;
      }
      else {
        func = but->block_create_func;
        arg = but->poin ? but->poin : but->func_argN;
      }
      break;
    case ButType::Menu:
      BLI_assert(but->menu_create_func);
      if (ui_but_menu_draw_as_popover(but)) {
        const char *idname = static_cast<const char *>(but->func_argN);
        popover_panel_type = WM_paneltype_find(idname, false);
      }

      if (popover_panel_type) {
        popoverfunc = but->menu_create_func;
      }
      else {
        menufunc = but->menu_create_func;
        arg = but->poin;
      }
      break;
    case ButType::Popover:
      BLI_assert(but->menu_create_func);
      popoverfunc = but->menu_create_func;
      popover_panel_type = reinterpret_cast<PanelType *>(but->poin);
      break;
    case ButType::Color:
      ui_but_v3_get(but, data->origvec);
      copy_v3_v3(data->vec, data->origvec);
      but->editvec = data->vec;

      if (ui_but_menu_draw_as_popover(but)) {
        const char *idname = static_cast<const char *>(but->func_argN);
        popover_panel_type = WM_paneltype_find(idname, false);
      }

      if (popover_panel_type) {
        popoverfunc = but->menu_create_func;
      }
      else {
        handlefunc = ui_block_func_COLOR;
      }
      arg = but;
      break;

      /* quiet warnings for unhandled types */
    default:
      break;
  }

  if (func || handlefunc) {
    data->menu = ui_popup_block_create(
        C, data->region, but, func, handlefunc, arg, nullptr, false);
    if (but->block->handle) {
      data->menu->popup = but->block->handle->popup;
    }
  }
  else if (menufunc) {
    data->menu = ui_popup_menu_create(C, data->region, but, menufunc, arg);
    if (MenuType *mt = UI_but_menutype_get(but)) {
      STRNCPY_UTF8(data->menu->menu_idname, mt->idname);
    }
    if (but->block->handle) {
      data->menu->popup = but->block->handle->popup;
    }
  }
  else if (popoverfunc) {
    data->menu = ui_popover_panel_create(C, data->region, but, popoverfunc, popover_panel_type);
    if (but->block->handle) {
      data->menu->popup = but->block->handle->popup;
    }
  }

#ifdef USE_ALLSELECT
  {
    if (IS_ALLSELECT_EVENT(data->window->eventstate)) {
      data->select_others.is_enabled = true;
    }
  }
#endif

  /* Force new region handler to run, in case that needs to activate some state (e.g. to handle
   * #UI_BUT2_FORCE_SEMI_MODAL_ACTIVE). */
  WM_event_add_mousemove(data->window);

  /* this makes adjacent blocks auto open from now on */
  // if (but->block->auto_open == 0) {
  //  but->block->auto_open = 1;
  //}
}

static void ui_block_open_end(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (but) {
    but->editval = nullptr;
    but->editvec = nullptr;

    but->block->auto_open_last = BLI_time_now_seconds();
  }

  ED_workspace_status_text(C, nullptr);

  if (data->menu) {
    ui_popup_block_free(C, data->menu);
    data->menu = nullptr;
  }
}

int ui_but_menu_direction(uiBut *but)
{
  uiHandleButtonData *data = but->active;

  if (data && data->menu) {
    return data->menu->direction;
  }

  return 0;
}

/**
 * Hack for #uiList #ButType::ListRow buttons to "give" events to overlaying #ButType::Text
 * buttons (Ctrl-Click rename feature & co).
 */
static uiBut *ui_but_list_row_text_activate(bContext *C,
                                            uiBut *but,
                                            uiHandleButtonData *data,
                                            const wmEvent *event,
                                            uiButtonActivateType activate_type)
{
  ARegion *region = data->region;
  uiBut *labelbut = ui_but_find_mouse_over_ex(region, event->xy, true, false, nullptr, nullptr);

  if (labelbut && labelbut->type == ButType::Text && !(labelbut->flag & UI_BUT_DISABLED)) {
    /* Exit list-row. */
    data->cancel = true;
    button_activate_exit(C, but, data, false, false);

    /* Activate the text button. */
    button_activate_init(C, region, labelbut, activate_type);

    return labelbut;
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Events for Various Button Types
 * \{ */

static uiButExtraOpIcon *ui_but_extra_operator_icon_mouse_over_get(uiBut *but,
                                                                   ARegion *region,
                                                                   const wmEvent *event)
{
  if (BLI_listbase_is_empty(&but->extra_op_icons)) {
    return nullptr;
  }

  int x = event->xy[0], y = event->xy[1];
  ui_window_to_block(region, but->block, &x, &y);
  if (!BLI_rctf_isect_pt(&but->rect, x, y)) {
    return nullptr;
  }

  const float icon_size = 0.8f * BLI_rctf_size_y(&but->rect); /* ICON_SIZE_FROM_BUTRECT */
  float xmax = but->rect.xmax;
  /* Same as in 'widget_draw_extra_icons', icon padding from the right edge. */
  xmax -= 0.2 * icon_size;

  /* Handle the padding space from the right edge as the last button. */
  if (x > xmax) {
    return static_cast<uiButExtraOpIcon *>(but->extra_op_icons.last);
  }

  /* Inverse order, from right to left. */
  LISTBASE_FOREACH_BACKWARD (uiButExtraOpIcon *, op_icon, &but->extra_op_icons) {
    if ((x > (xmax - icon_size)) && x <= xmax) {
      return op_icon;
    }
    xmax -= icon_size;
  }

  return nullptr;
}

static bool ui_do_but_extra_operator_icon(bContext *C,
                                          uiBut *but,
                                          uiHandleButtonData *data,
                                          const wmEvent *event)
{
  uiButExtraOpIcon *op_icon = ui_but_extra_operator_icon_mouse_over_get(but, data->region, event);

  if (!op_icon) {
    return false;
  }

  /* Only act on release, avoids some glitches. */
  if (event->val != KM_RELEASE) {
    /* Still swallow events on the icon. */
    return true;
  }

  ED_region_tag_redraw(data->region);
  button_tooltip_timer_reset(C, but);

  ui_but_extra_operator_icon_apply(C, but, op_icon);
  /* NOTE: 'but', 'data' may now be freed, don't access. */

  return true;
}

static void ui_do_but_extra_operator_icons_mousemove(uiBut *but,
                                                     uiHandleButtonData *data,
                                                     const wmEvent *event)
{
  uiButExtraOpIcon *old_highlighted = nullptr;

  /* Unset highlighting of all first. */
  LISTBASE_FOREACH (uiButExtraOpIcon *, op_icon, &but->extra_op_icons) {
    if (op_icon->highlighted) {
      old_highlighted = op_icon;
    }
    op_icon->highlighted = false;
  }

  uiButExtraOpIcon *hovered = ui_but_extra_operator_icon_mouse_over_get(but, data->region, event);

  if (hovered) {
    hovered->highlighted = true;
  }

  if (old_highlighted != hovered) {
    ED_region_tag_redraw_no_rebuild(data->region);
  }
}

#ifdef USE_DRAG_TOGGLE
/* Shared by any button that supports drag-toggle. */
static bool ui_do_but_ANY_drag_toggle(
    bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event, int *r_retval)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS && ui_but_is_drag_toggle(but)) {
      ui_apply_but(C, but->block, but, data, true);
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->xy[0];
      data->dragstarty = event->xy[1];
      *r_retval = WM_UI_HANDLER_BREAK;
      return true;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_DRAG) {
    /* NOTE: the 'BUTTON_STATE_WAIT_DRAG' part of 'ui_do_but_EXIT' could be refactored into
     * its own function */
    data->applied = false;
    *r_retval = ui_do_but_EXIT(C, but, data, event);
    return true;
  }
  return false;
}
#endif /* USE_DRAG_TOGGLE */

static int ui_do_but_BUT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
#ifdef USE_DRAG_TOGGLE
  {
    int retval;
    if (ui_do_but_ANY_drag_toggle(C, but, data, event, &retval)) {
      return retval;
    }
  }
#endif

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_RELEASE);
      return WM_UI_HANDLER_BREAK;
    }
    if (event->type == LEFTMOUSE && event->val == KM_RELEASE && but->block->handle) {
      /* regular buttons will be 'UI_SELECT', menu items 'UI_HOVER' */
      if (!(but->flag & (UI_SELECT | UI_HOVER))) {
        data->cancel = true;
      }
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
    if (ELEM(event->type, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_FLASH);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_RELEASE) {
    if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (!(but->flag & UI_SELECT)) {
        data->cancel = true;
      }
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_HOTKEYEVT(bContext *C,
                               uiBut *but,
                               uiHandleButtonData *data,
                               const wmEvent *event)
{
  uiButHotkeyEvent *hotkey_but = (uiButHotkeyEvent *)but;
  BLI_assert(but->type == ButType::HotkeyEvent);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY, EVT_BUT_OPEN) &&
        (event->val == KM_PRESS))
    {
      but->drawstr.clear();
      hotkey_but->modifier_key = wmEventModifierFlag(0);
      button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
    if (ISMOUSE_MOTION(event->type)) {
      return WM_UI_HANDLER_CONTINUE;
    }
    if (event->type == EVT_UNKNOWNKEY) {
      WM_global_report(RPT_WARNING, "Unsupported key: Unknown");
      return WM_UI_HANDLER_CONTINUE;
    }
    if (event->type == EVT_CAPSLOCKKEY) {
      WM_global_report(RPT_WARNING, "Unsupported key: CapsLock");
      return WM_UI_HANDLER_CONTINUE;
    }

    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      /* only cancel if click outside the button */
      if (ui_but_contains_point_px(but, but->active->region, event->xy) == false) {
        data->cancel = true;
        /* Close the containing popup (if any). */
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        return WM_UI_HANDLER_BREAK;
      }
    }

    /* always set */
    hotkey_but->modifier_key = event->modifier;

    ui_but_update(but);
    ED_region_tag_redraw(data->region);

    if (event->val == KM_PRESS) {
      if (ISHOTKEY(event->type) && (event->type != EVT_ESCKEY)) {
        if (WM_key_event_string(event->type, false)[0]) {
          ui_but_value_set(but, event->type);
        }
        else {
          data->cancel = true;
        }

        button_activate_state(C, but, BUTTON_STATE_EXIT);
        return WM_UI_HANDLER_BREAK;
      }
      if (event->type == EVT_ESCKEY) {
        if (event->val == KM_PRESS) {
          data->cancel = true;
          data->escapecancel = true;
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
      }
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_KEYEVT(bContext *C,
                            uiBut *but,
                            uiHandleButtonData *data,
                            const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
    if (ISMOUSE_MOTION(event->type)) {
      return WM_UI_HANDLER_CONTINUE;
    }

    if (event->val == KM_PRESS) {
      if (WM_key_event_string(event->type, false)[0]) {
        ui_but_value_set(but, event->type);
      }
      else {
        data->cancel = true;
      }

      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_TAB(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  const bool is_property = (but->rnaprop != nullptr);

#ifdef USE_DRAG_TOGGLE
  if (is_property) {
    int retval;
    if (ui_do_but_ANY_drag_toggle(C, but, data, event, &retval)) {
      return retval;
    }
  }
#endif

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    const int rna_type = but->rnaprop ? RNA_property_type(but->rnaprop) : 0;

    if (is_property && ELEM(rna_type, PROP_POINTER, PROP_STRING) &&
        (but->custom_data != nullptr) && (event->type == LEFTMOUSE) &&
        ((event->val == KM_DBL_CLICK) || (event->modifier & KM_CTRL)))
    {
      button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      return WM_UI_HANDLER_BREAK;
    }
    if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY)) {
      const int event_val = (is_property) ? KM_PRESS : KM_CLICK;
      if (event->val == event_val) {
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        return WM_UI_HANDLER_BREAK;
      }
    }
  }
  else if (data->state == BUTTON_STATE_TEXT_EDITING) {
    ui_do_but_textedit(C, block, but, data, event);
    return WM_UI_HANDLER_BREAK;
  }
  else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
    ui_do_but_textedit_select(C, block, but, data, event);
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

/**
 * Increment or decrement an integer value within
 * the text of a button while hovering over it.
 */
static int ui_do_but_text_value_cycle(bContext *C,
                                      uiBut *but,
                                      uiHandleButtonData *data,
                                      const int inc_value)
{
  /* The allocated string only increases in length by 1,
   * only support incrementing by one. */
  BLI_assert(ELEM(inc_value, -1, 1));

  if (data->state != BUTTON_STATE_HIGHLIGHT) {
    /* This function assumes the mouse is only hovering over the input. */
    return WM_UI_HANDLER_CONTINUE;
  }

  /* Retrieve the string. */
  char *but_string;
  int str_maxncpy = ui_but_string_get_maxncpy(but);
  bool no_zero_strip = false;
  if (str_maxncpy != 0) {
    but_string = MEM_calloc_arrayN<char>(str_maxncpy, __func__);
    ui_but_string_get_ex(
        but, but_string, str_maxncpy, UI_PRECISION_FLOAT_MAX, true, &no_zero_strip);
  }
  else {
    but_string = ui_but_string_get_dynamic(but, &str_maxncpy);
  }

  if (but_string[0] == '\0') {
    /* Don't append a number to an empty string. */
    MEM_freeN(but_string);
    return WM_UI_HANDLER_CONTINUE;
  }

  /* More space needed for an added digit. */
  str_maxncpy += 1;
  char *head = MEM_calloc_arrayN<char>(str_maxncpy, __func__);
  char *tail = MEM_calloc_arrayN<char>(str_maxncpy, __func__);
  ushort digits;

  /* Decode the string, parsing head, digits, tail. */
  int num = BLI_path_sequence_decode(but_string, head, str_maxncpy, tail, str_maxncpy, &digits);
  MEM_freeN(but_string);
  if (num == 0 && digits == 0) {
    BLI_str_rstrip_digits(head);
  }

  /* Increase or decrease the value. */
  num += inc_value;

  /* Encode the new string with the changed value. */
  char *string = MEM_calloc_arrayN<char>(str_maxncpy, __func__);
  BLI_path_sequence_encode(string, str_maxncpy, head, tail, digits, num);

  /* Save this new string to the button. */
  ui_but_set_string_interactive(C, but, string);

  /* Free the strings. */
  MEM_freeN(string);
  MEM_freeN(head);
  MEM_freeN(tail);

  return WM_UI_HANDLER_BREAK;
}

static int ui_do_but_TEX(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, EVT_PADENTER, EVT_RETKEY) &&
        event->val == KM_PRESS)
    {
      if (ELEM(event->type, EVT_PADENTER, EVT_RETKEY) && !UI_but_is_utf8(but)) {
        /* Pass, allow file-selector, enter to execute. */
      }
      else if (ELEM(but->emboss,
                    blender::ui::EmbossType::None,
                    blender::ui::EmbossType::NoneOrStatus) &&
               (event->modifier != KM_CTRL))
      {
        /* Pass. */
      }
      else {
        if (!ui_but_extra_operator_icon_mouse_over_get(but, data->region, event)) {
          button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        }
        return WM_UI_HANDLER_BREAK;
      }
    }
    else if (ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE) && (event->modifier & KM_CTRL)) {
      const int inc_value = (event->type == WHEELUPMOUSE) ? 1 : -1;
      return ui_do_but_text_value_cycle(C, but, data, inc_value);
    }
  }
  else if (data->state == BUTTON_STATE_TEXT_EDITING) {
    return ui_do_but_textedit(C, block, but, data, event);
  }
  else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
    return ui_do_but_textedit_select(C, block, but, data, event);
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_SEARCH_UNLINK(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  /* unlink icon is on right */
  if (ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, EVT_PADENTER, EVT_RETKEY)) {
    /* doing this on KM_PRESS calls eyedropper after clicking unlink icon */
    if ((event->val == KM_RELEASE) && ui_do_but_extra_operator_icon(C, but, data, event)) {
      return WM_UI_HANDLER_BREAK;
    }
  }
  return ui_do_but_TEX(C, block, but, data, event);
}

static int ui_do_but_TOG(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
#ifdef USE_DRAG_TOGGLE
  {
    int retval;
    if (ui_do_but_ANY_drag_toggle(C, but, data, event, &retval)) {
      return retval;
    }
  }
#endif

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    bool do_activate = false;
    if (ELEM(event->type, EVT_PADENTER, EVT_RETKEY)) {
      if (event->val == KM_PRESS) {
        do_activate = true;
      }
    }
    else if (event->type == LEFTMOUSE) {
      if (ui_block_is_menu(but->block)) {
        /* Behave like other menu items. */
        do_activate = (event->val == KM_RELEASE);
      }
      else if (!ui_do_but_extra_operator_icon(C, but, data, event)) {
        /* Also use double-clicks to prevent fast clicks to leak to other handlers (#76481). */
        do_activate = ELEM(event->val, KM_PRESS, KM_DBL_CLICK);
      }
    }

    if (do_activate) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
    if (ELEM(event->type, MOUSEPAN, WHEELDOWNMOUSE, WHEELUPMOUSE) && (event->modifier & KM_CTRL)) {
      if (ELEM(but->type,
               ButType::Toggle,
               ButType::ToggleN,
               ButType::IconToggle,
               ButType::IconToggleN,
               ButType::ButToggle,
               ButType::Checkbox,
               ButType::CheckboxN))
      {
        /* Support Ctrl-Wheel to cycle toggles and check-boxes. */
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        return WM_UI_HANDLER_BREAK;
      }
      else if (but->type == ButType::Row) {
        /* Support Ctrl-Wheel to cycle values on expanded enum rows. */
        int type = event->type;
        int val = event->val;

        /* Convert pan to scroll-wheel. */
        if (type == MOUSEPAN) {
          ui_pan_to_scroll(event, &type, &val);

          if (type == MOUSEPAN) {
            return WM_UI_HANDLER_BREAK;
          }
        }

        const int direction = (type == WHEELDOWNMOUSE) ? -1 : 1;
        uiBut *but_select = ui_but_find_select_in_enum(but, direction);
        if (but_select) {
          uiBut *but_other = (direction == -1) ? but_select->block->next_but(but_select) :
                                                 but_select->block->prev_but(but_select);
          if (but_other && ui_but_find_select_in_enum__cmp(but, but_other)) {
            ARegion *region = data->region;

            data->cancel = true;
            button_activate_exit(C, but, data, false, false);

            /* Activate the text button. */
            button_activate_init(C, region, but_other, BUTTON_ACTIVATE_OVER);
            data = but_other->active;
            if (data) {
              ui_apply_but(C, but->block, but_other, but_other->active, true);
              button_activate_exit(C, but_other, data, false, false);

              /* restore active button */
              button_activate_init(C, region, but, BUTTON_ACTIVATE_OVER);
            }
            else {
              /* shouldn't happen */
              BLI_assert(0);
            }
          }
        }
        return WM_UI_HANDLER_BREAK;
      }
    }
  }
  return WM_UI_HANDLER_CONTINUE;
}

/**
 * \param close_popup: In most cases activating the view item should close the popup it is in
 *                     (unless #AbstractView::keep_open() was called when building the view), if
 *                     any. But this should only be done when activating the view item directly,
 *                     things like clicking nested buttons or calling the context menu should keep
 *                     the popup open for further interaction.
 */
static void force_activate_view_item_but(bContext *C,
                                         ARegion *region,
                                         uiButViewItem *but,
                                         const bool close_popup = true)
{

  /* For popups. Other abstract view instances correctly calls the select operator, see:
   * #141235. */
  if (but->context) {
    CTX_store_set(C, but->context);
  }
  but->view_item->activate(*C);
  CTX_store_set(C, nullptr);

  ED_region_tag_redraw_no_rebuild(region);
  ED_region_tag_refresh_ui(region);

  if (close_popup && !UI_view_item_popup_keep_open(*but->view_item)) {
    UI_popup_menu_close_from_but(but);
  }
}

static int ui_do_but_VIEW_ITEM(bContext *C,
                               uiBut *but,
                               uiHandleButtonData *data,
                               const wmEvent *event)
{
  uiButViewItem *view_item_but = (uiButViewItem *)but;
  BLI_assert(view_item_but->type == ButType::ViewItem);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE) {
      switch (event->val) {
        case KM_PRESS:
          /* Extra icons have priority, don't mess with them. */
          if (ui_but_extra_operator_icon_mouse_over_get(but, data->region, event)) {
            return WM_UI_HANDLER_BREAK;
          }

          if (ui_block_is_popup_any(but->block)) {
            /* TODO(!147047): This should be handled in selection operator. */
            force_activate_view_item_but(C, data->region, view_item_but, false);
            return WM_UI_HANDLER_BREAK;
          }

          if (UI_view_item_supports_drag(*view_item_but->view_item)) {
            button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
            data->dragstartx = event->xy[0];
            data->dragstarty = event->xy[1];
          }

          /* Always continue for drag and drop handling. Also for cases where keymap items are
           * registered to add custom activate or drag operators (the pose library does this for
           * example). */
          return WM_UI_HANDLER_CONTINUE;
        case KM_DBL_CLICK:
          if (UI_view_item_can_rename(*view_item_but->view_item)) {
            data->cancel = true;
            UI_view_item_begin_rename(*view_item_but->view_item);
            ED_region_tag_redraw(data->region);
            ED_region_tag_refresh_ui(data->region);
            return WM_UI_HANDLER_BREAK;
          }
          return WM_UI_HANDLER_CONTINUE;
      }
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_DRAG) {
    /* Let "default" button handling take care of the drag logic. */
    return ui_do_but_EXIT(C, but, data, event);
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_EXIT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {

    /* First handle click on icon-drag type button. */
    if ((event->type == LEFTMOUSE) && (event->val == KM_PRESS) && ui_but_drag_is_draggable(but)) {
      if (ui_but_contains_point_px_icon(but, data->region, event)) {

        /* tell the button to wait and keep checking further events to
         * see if it should start dragging */
        button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
        data->dragstartx = event->xy[0];
        data->dragstarty = event->xy[1];
        return WM_UI_HANDLER_CONTINUE;
      }
    }
#ifdef USE_DRAG_TOGGLE
    if ((event->type == LEFTMOUSE) && (event->val == KM_PRESS) && ui_but_is_drag_toggle(but)) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->xy[0];
      data->dragstarty = event->xy[1];
      return WM_UI_HANDLER_CONTINUE;
    }
#endif

    if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
      int ret = WM_UI_HANDLER_BREAK;
      /* XXX: (a bit ugly) Special case handling for file-browser drag buttons (icon and filename
       * label). */
      if (ui_but_drag_is_draggable(but) && ui_but_contains_point_px_icon(but, data->region, event))
      {
        ret = WM_UI_HANDLER_CONTINUE;
      }
      const uiBut *view_but = ui_view_item_find_mouse_over(data->region, event->xy);
      if (view_but) {
        ret = WM_UI_HANDLER_CONTINUE;
      }
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return ret;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_DRAG) {

    /* this function also ends state */
    if (ui_but_drag_init(C, but, data, event)) {
      return WM_UI_HANDLER_BREAK;
    }

    /* If the mouse has been pressed and released, getting to
     * this point without triggering a drag, then clear the
     * drag state for this button and continue to pass on the event */
    if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_CONTINUE;
    }

    /* while waiting for a drag to be triggered, always block
     * other events from getting handled */
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

/* var names match ui_numedit_but_NUM */
static float ui_numedit_apply_snapf(
    uiBut *but, float tempf, float softmin, float softmax, const enum eSnapType snap)
{
  if (tempf == softmin || tempf == softmax || snap == SNAP_OFF) {
    /* pass */
  }
  else {
    const PropertyScaleType scale_type = ui_but_scale_type(but);
    float softrange = softmax - softmin;
    float fac = 1.0f;

    if (ui_but_is_unit(but)) {
      const UnitSettings *unit = but->block->unit;
      const int unit_type = RNA_SUBTYPE_UNIT_VALUE(UI_but_unit_type_get(but));

      if (BKE_unit_is_valid(unit->system, unit_type)) {
        fac = float(BKE_unit_base_scalar(unit->system, unit_type));
        if (ELEM(unit_type, B_UNIT_LENGTH, B_UNIT_AREA, B_UNIT_VOLUME)) {
          fac /= unit->scale_length;
        }
      }
    }

    if (fac != 1.0f) {
      /* snap in unit-space */
      tempf /= fac;
      // softmin /= fac; /* UNUSED */
      // softmax /= fac; /* UNUSED */
      softrange /= fac;
    }

    /* workaround, too high snapping values */
    /* snapping by 10's for float buttons is quite annoying (location, scale...),
     * but allow for rotations */
    if (softrange >= 21.0f) {
      const UnitSettings *unit = but->block->unit;
      const int unit_type = UI_but_unit_type_get(but);
      if ((unit_type == PROP_UNIT_ROTATION) && (unit->system_rotation != USER_UNIT_ROT_RADIANS)) {
        /* Pass (degrees). */
      }
      else {
        softrange = 20.0f;
      }
    }

    BLI_assert(ELEM(snap, SNAP_ON, SNAP_ON_SMALL));
    switch (scale_type) {
      case PROP_SCALE_LINEAR:
      case PROP_SCALE_CUBIC: {
        const float snap_fac = (snap == SNAP_ON_SMALL ? 0.1f : 1.0f);
        if (softrange < 2.10f) {
          tempf = roundf(tempf * 10.0f / snap_fac) * 0.1f * snap_fac;
        }
        else if (softrange < 21.0f) {
          tempf = roundf(tempf / snap_fac) * snap_fac;
        }
        else {
          tempf = roundf(tempf * 0.1f / snap_fac) * 10.0f * snap_fac;
        }
        break;
      }
      case PROP_SCALE_LOG: {
        const float snap_fac = powf(10.0f,
                                    roundf(log10f(tempf) + UI_PROP_SCALE_LOG_SNAP_OFFSET) -
                                        (snap == SNAP_ON_SMALL ? 2.0f : 1.0f));
        tempf = roundf(tempf / snap_fac) * snap_fac;
        break;
      }
    }

    if (fac != 1.0f) {
      tempf *= fac;
    }
  }

  return tempf;
}

static float ui_numedit_apply_snap(int temp,
                                   float softmin,
                                   float softmax,
                                   const enum eSnapType snap)
{
  if (ELEM(temp, softmin, softmax)) {
    return temp;
  }

  switch (snap) {
    case SNAP_OFF:
      break;
    case SNAP_ON:
      temp = 10 * (temp / 10);
      break;
    case SNAP_ON_SMALL:
      temp = 100 * (temp / 100);
      break;
  }

  return temp;
}

static bool ui_numedit_but_NUM(uiButNumber *but,
                               uiHandleButtonData *data,
                               int mx,
                               blender::FunctionRef<int()> drag_threshold_fn,
                               const bool is_motion,
                               const enum eSnapType snap,
                               float fac)
{
  float deler, tempf;
  int lvalue, temp;
  bool changed = false;
  const bool is_float = ui_but_is_float(but);
  const PropertyScaleType scale_type = ui_but_scale_type(but);

  /* prevent unwanted drag adjustments, test motion so modifier keys refresh. */
  if ((is_motion || data->draglock) &&
      (ui_but_dragedit_update_mval(data, mx, drag_threshold_fn) == false))
  {
    return changed;
  }

  ui_block_interaction_begin_ensure(
      static_cast<bContext *>(but->block->evil_C), but->block, data, false);

  if (ui_but_is_cursor_warp(but)) {
    const float softmin = but->softmin;
    const float softmax = but->softmax;
    const float softrange = softmax - softmin;

    const float log_min = (scale_type == PROP_SCALE_LOG) ?
                              max_ff(max_ff(softmin, UI_PROP_SCALE_LOG_MIN),
                                     powf(10, -but->precision) * 0.5f) :
                              0;

    /* Mouse location isn't screen clamped to the screen so use a linear mapping
     * 2px == 1-int, or 1px == 1-ClickStep */
    if (is_float) {
      fac *= 0.01f * but->step_size;
      switch (scale_type) {
        case PROP_SCALE_LINEAR: {
          tempf = float(data->startvalue) + float(mx - data->dragstartx) * fac;
          break;
        }
        case PROP_SCALE_LOG: {
          const float startvalue = max_ff(float(data->startvalue), log_min);
          tempf = expf(float(mx - data->dragstartx) * fac) * startvalue;
          if (tempf <= log_min) {
            tempf = 0.0f;
          }
          break;
        }
        case PROP_SCALE_CUBIC: {
          tempf = cbrtf(float(data->startvalue)) + float(mx - data->dragstartx) * fac;
          tempf *= tempf * tempf;
          break;
        }
      }

      tempf = ui_numedit_apply_snapf(but, tempf, softmin, softmax, snap);

#if 1 /* fake moving the click start, nicer for dragging back after passing the limit */
      switch (scale_type) {
        case PROP_SCALE_LINEAR: {
          if (tempf < softmin) {
            data->dragstartx -= (softmin - tempf) / fac;
            tempf = softmin;
          }
          else if (tempf > softmax) {
            data->dragstartx -= (softmax - tempf) / fac;
            tempf = softmax;
          }
          break;
        }
        case PROP_SCALE_LOG: {
          const float startvalue = max_ff(float(data->startvalue), log_min);
          if (tempf < log_min) {
            data->dragstartx -= logf(log_min / startvalue) / fac - float(mx - data->dragstartx);
            tempf = softmin;
          }
          else if (tempf > softmax) {
            data->dragstartx -= logf(softmax / startvalue) / fac - float(mx - data->dragstartx);
            tempf = softmax;
          }
          break;
        }
        case PROP_SCALE_CUBIC: {
          if (tempf < softmin) {
            data->dragstartx = mx - int((cbrtf(softmin) - cbrtf(float(data->startvalue))) / fac);
            tempf = softmin;
          }
          else if (tempf > softmax) {
            data->dragstartx = mx - int((cbrtf(softmax) - cbrtf(float(data->startvalue))) / fac);
            tempf = softmax;
          }
          break;
        }
      }
#else
      CLAMP(tempf, softmin, softmax);
#endif

      if (tempf != float(data->value)) {
        data->dragchange = true;
        data->value = tempf;
        changed = true;
      }
    }
    else {
      if (softrange > 256) {
        fac = 1.0;
      } /* 1px == 1 */
      else if (softrange > 32) {
        fac = 1.0 / 2.0;
      } /* 2px == 1 */
      else {
        fac = 1.0 / 16.0;
      } /* 16px == 1? */

      temp = data->startvalue + ((double(mx) - data->dragstartx) * double(fac));
      temp = ui_numedit_apply_snap(temp, softmin, softmax, snap);

#if 1 /* fake moving the click start, nicer for dragging back after passing the limit */
      if (temp < softmin) {
        data->dragstartx -= (softmin - temp) / fac;
        temp = softmin;
      }
      else if (temp > softmax) {
        data->dragstartx += (temp - softmax) / fac;
        temp = softmax;
      }
#else
      CLAMP(temp, softmin, softmax);
#endif

      if (temp != data->value) {
        data->dragchange = true;
        data->value = temp;
        changed = true;
      }
    }

    data->draglastx = mx;
  }
  else {
    /* Use 'but->softmin', 'but->softmax' when clamping values. */
    const float softmin = data->drag_map_soft_min;
    const float softmax = data->drag_map_soft_max;
    const float softrange = softmax - softmin;

    float non_linear_range_limit;
    float non_linear_pixel_map;
    float non_linear_scale;

    /* Use a non-linear mapping of the mouse drag especially for large floats
     * (normal behavior) */
    deler = 500;
    if (is_float) {
      /* not needed for smaller float buttons */
      non_linear_range_limit = 11.0f;
      non_linear_pixel_map = 500.0f;
    }
    else {
      /* only scale large int buttons */
      non_linear_range_limit = 129.0f;
      /* Larger for ints, we don't need to fine tune them. */
      non_linear_pixel_map = 250.0f;

      /* prevent large ranges from getting too out of control */
      if (softrange > 600) {
        deler = powf(softrange, 0.75f);
      }
      else if (softrange < 25) {
        deler = 50.0;
      }
      else if (softrange < 100) {
        deler = 100.0;
      }
    }
    deler /= fac;

    if (softrange > non_linear_range_limit) {
      non_linear_scale = float(abs(mx - data->dragstartx)) / non_linear_pixel_map;
    }
    else {
      non_linear_scale = 1.0f;
    }

    if (is_float == false) {
      /* at minimum, moving cursor 2 pixels should change an int button. */
      CLAMP_MIN(non_linear_scale, 0.5f * UI_SCALE_FAC);
    }

    data->dragf += (float(mx - data->draglastx) / deler) * non_linear_scale;

    if (but->softmin == softmin) {
      CLAMP_MIN(data->dragf, 0.0f);
    }
    if (but->softmax == softmax) {
      CLAMP_MAX(data->dragf, 1.0f);
    }

    data->draglastx = mx;

    switch (scale_type) {
      case PROP_SCALE_LINEAR: {
        tempf = (softmin + data->dragf * softrange);
        break;
      }
      case PROP_SCALE_LOG: {
        const float log_min = max_ff(max_ff(softmin, UI_PROP_SCALE_LOG_MIN),
                                     powf(10.0f, -but->precision) * 0.5f);
        const float base = softmax / log_min;
        tempf = powf(base, data->dragf) * log_min;
        if (tempf <= log_min) {
          tempf = 0.0f;
        }
        break;
      }
      case PROP_SCALE_CUBIC: {
        tempf = (softmin + data->dragf * softrange);
        tempf *= tempf * tempf;
        float cubic_min = softmin * softmin * softmin;
        float cubic_max = softmax * softmax * softmax;
        tempf = (tempf - cubic_min) / (cubic_max - cubic_min) * softrange + softmin;
        break;
      }
    }

    if (!is_float) {
      temp = round_fl_to_int(tempf);

      temp = ui_numedit_apply_snap(temp, but->softmin, but->softmax, snap);

      CLAMP(temp, but->softmin, but->softmax);
      lvalue = int(data->value);

      if (temp != lvalue) {
        data->dragchange = true;
        data->value = double(temp);
        changed = true;
      }
    }
    else {
      temp = 0;
      tempf = ui_numedit_apply_snapf(but, tempf, but->softmin, but->softmax, snap);

      CLAMP(tempf, but->softmin, but->softmax);

      if (tempf != float(data->value)) {
        data->dragchange = true;
        data->value = tempf;
        changed = true;
      }
    }
  }

  return changed;
}

static void ui_numedit_set_active(uiBut *but)
{
  const int oldflag = but->drawflag;
  but->drawflag &= ~(UI_BUT_HOVER_LEFT | UI_BUT_HOVER_RIGHT);

  uiHandleButtonData *data = but->active;
  if (!data) {
    return;
  }

  /* Ignore once we start dragging. */
  if (data->dragchange == false) {
    const float handle_width = min_ff(BLI_rctf_size_x(&but->rect) / 3,
                                      BLI_rctf_size_y(&but->rect) * 0.7f);
    /* we can click on the side arrows to increment/decrement,
     * or click inside to edit the value directly */
    int mx = data->window->eventstate->xy[0];
    int my = data->window->eventstate->xy[1];
    ui_window_to_block(data->region, but->block, &mx, &my);

    if (mx < (but->rect.xmin + handle_width)) {
      but->drawflag |= UI_BUT_HOVER_LEFT;
    }
    else if (mx > (but->rect.xmax - handle_width)) {
      but->drawflag |= UI_BUT_HOVER_RIGHT;
    }
  }

  /* Don't change the cursor once pressed. */
  if ((but->flag & UI_SELECT) == 0) {
    if ((but->drawflag & UI_BUT_HOVER_LEFT) || (but->drawflag & UI_BUT_HOVER_RIGHT)) {
      if (data->changed_cursor) {
        WM_cursor_modal_restore(data->window);
        data->changed_cursor = false;
      }
    }
    else {
      if (data->changed_cursor == false) {
        WM_cursor_modal_set(data->window, WM_CURSOR_X_MOVE);
        data->changed_cursor = true;
      }
    }
  }

  if (but->drawflag != oldflag) {
    ED_region_tag_redraw(data->region);
  }
}

static int ui_do_but_NUM(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  uiButNumber *number_but = (uiButNumber *)but;
  int click = 0;
  int retval = WM_UI_HANDLER_CONTINUE;

  /* mouse location scaled to fit the UI */
  int mx = event->xy[0];
  int my = event->xy[1];
  /* mouse location kept at screen pixel coords */
  const int screen_mx = event->xy[0];

  /* Defer evaluation as it's rarely needed. */
  auto drag_threshold_fn = [event]() -> int { return WM_event_drag_threshold(event); };

  BLI_assert(but->type == ButType::Num);

  ui_window_to_block(data->region, block, &mx, &my);
  ui_numedit_set_active(but);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    int type = event->type, val = event->val;

    if (type == MOUSEPAN) {
      ui_pan_to_scroll(event, &type, &val);
    }

    /* XXX hardcoded keymap check.... */
    if (type == MOUSEPAN && (event->modifier & KM_CTRL)) {
      /* allow accumulating values, otherwise scrolling gets preference */
      retval = WM_UI_HANDLER_BREAK;
    }
    else if (type == WHEELDOWNMOUSE && (event->modifier & KM_CTRL)) {
      mx = but->rect.xmin;
      but->drawflag &= ~UI_BUT_HOVER_RIGHT;
      but->drawflag |= UI_BUT_HOVER_LEFT;
      click = 1;
    }
    else if ((type == WHEELUPMOUSE) && (event->modifier & KM_CTRL)) {
      mx = but->rect.xmax;
      but->drawflag &= ~UI_BUT_HOVER_LEFT;
      but->drawflag |= UI_BUT_HOVER_RIGHT;
      click = 1;
    }
    else if (event->val == KM_PRESS) {
      if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && (event->modifier & KM_CTRL)) {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
      else if (event->type == LEFTMOUSE) {
        data->dragstartx = data->draglastx = ui_but_is_cursor_warp(but) ? screen_mx : mx;
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
      else if (ELEM(event->type, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
        click = 1;
      }
      else if (event->type == EVT_BUT_OPEN) {
        /* Handle UI_but_focus_on_enter_event. */
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
      else if (event->type == EVT_MINUSKEY && event->val == KM_PRESS) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        data->value = -data->value;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
      }

#ifdef USE_DRAG_MULTINUM
      copy_v2_v2_int(data->multi_data.drag_start, event->xy);
#endif
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (data->dragchange) {
#ifdef USE_DRAG_MULTINUM
        /* If we started multi-button but didn't drag, then edit. */
        if (data->multi_data.init == uiHandleButtonMulti::INIT_SETUP) {
          click = 1;
        }
        else
#endif
        {
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
      }
      else {
        click = 1;
      }
    }
    else if ((event->type == MOUSEMOVE) || ui_event_is_snap(event)) {
      const bool is_motion = (event->type == MOUSEMOVE);
      const enum eSnapType snap = ui_event_to_snap(event);
      float fac;

#ifdef USE_DRAG_MULTINUM
      data->multi_data.drag_dir[0] += abs(data->draglastx - mx);
      data->multi_data.drag_dir[1] += abs(data->draglasty - my);
#endif

      fac = 1.0f;
      if (event->modifier & KM_SHIFT) {
        fac /= 10.0f;
      }

      if (ui_numedit_but_NUM(number_but,
                             data,
                             (ui_but_is_cursor_warp(but) ? screen_mx : mx),
                             drag_threshold_fn,
                             is_motion,
                             snap,
                             fac))
      {
        ui_numedit_apply(C, block, but, data);
      }
#ifdef USE_DRAG_MULTINUM
      else if (data->multi_data.has_mbuts) {
        if (data->multi_data.init == uiHandleButtonMulti::INIT_ENABLE) {
          ui_multibut_states_apply(C, data, block);
        }
      }
#endif
    }
    retval = WM_UI_HANDLER_BREAK;
  }
  else if (data->state == BUTTON_STATE_TEXT_EDITING) {
    ui_do_but_textedit(C, block, but, data, event);
    retval = WM_UI_HANDLER_BREAK;
  }
  else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
    ui_do_but_textedit_select(C, block, but, data, event);
    retval = WM_UI_HANDLER_BREAK;
  }

  if (click) {
    /* we can click on the side arrows to increment/decrement,
     * or click inside to edit the value directly */

    if (!ui_but_is_float(but)) {
      /* Integer Value. */
      if (but->drawflag & (UI_BUT_HOVER_LEFT | UI_BUT_HOVER_RIGHT)) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

        const int value_step = int(number_but->step_size);
        BLI_assert(value_step > 0);
        const int softmin = round_fl_to_int_clamp(but->softmin);
        const int softmax = round_fl_to_int_clamp(but->softmax);
        const double value_test = (but->drawflag & UI_BUT_HOVER_LEFT) ?
                                      double(max_ii(softmin, int(data->value) - value_step)) :
                                      double(min_ii(softmax, int(data->value) + value_step));
        if (value_test != data->value) {
          data->value = double(value_test);
        }
        else {
          data->cancel = true;
        }
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
      else {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      }
    }
    else {
      /* Float Value. */
      if (but->drawflag & (UI_BUT_HOVER_LEFT | UI_BUT_HOVER_RIGHT)) {
        const PropertyScaleType scale_type = ui_but_scale_type(but);

        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

        double value_step;
        if (scale_type == PROP_SCALE_LOG) {
          double precision = (roundf(log10f(data->value) + UI_PROP_SCALE_LOG_SNAP_OFFSET) - 1.0f) +
                             log10f(number_but->step_size);
          /* Non-finite when `data->value` is zero. */
          if (UNLIKELY(!isfinite(precision))) {
            precision = -FLT_MAX; /* Ignore this value. */
          }
          value_step = powf(10.0f, max_ff(precision, -number_but->precision));
        }
        else {
          value_step = double(number_but->step_size * UI_PRECISION_FLOAT_SCALE);
        }
        BLI_assert(value_step > 0.0f);
        const double value_test =
            (but->drawflag & UI_BUT_HOVER_LEFT) ?
                double(max_ff(but->softmin, float(data->value - value_step))) :
                double(min_ff(but->softmax, float(data->value + value_step)));
        if (value_test != data->value) {
          data->value = value_test;
        }
        else {
          data->cancel = true;
        }
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
      else {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      }
    }

    retval = WM_UI_HANDLER_BREAK;
  }

  data->draglastx = mx;
  data->draglasty = my;

  return retval;
}

static bool ui_numedit_but_SLI(uiBut *but,
                               uiHandleButtonData *data,
                               int mx,
                               blender::FunctionRef<int()> drag_threshold_fn,
                               const bool is_horizontal,
                               const bool is_motion,
                               const bool snap,
                               const bool shift)
{
  float cursor_x_range, f, tempf, softmin, softmax, softrange;
  int temp, lvalue;
  bool changed = false;
  float mx_fl, my_fl;

  /* prevent unwanted drag adjustments, test motion so modifier keys refresh. */
  if ((but->type != ButType::Scroll) && (is_motion || data->draglock) &&
      (ui_but_dragedit_update_mval(data, mx, drag_threshold_fn) == false))
  {
    return changed;
  }

  ui_block_interaction_begin_ensure(
      static_cast<bContext *>(but->block->evil_C), but->block, data, false);

  const PropertyScaleType scale_type = ui_but_scale_type(but);

  softmin = but->softmin;
  softmax = but->softmax;
  softrange = softmax - softmin;

  /* yes, 'mx' as both x/y is intentional */
  ui_mouse_scale_warp(data, mx, mx, &mx_fl, &my_fl, shift);

  if (but->type == ButType::NumSlider) {
    cursor_x_range = BLI_rctf_size_x(&but->rect);
  }
  else if (but->type == ButType::Scroll) {
    uiButScrollBar *scroll_but = reinterpret_cast<uiButScrollBar *>(but);
    const float size = (is_horizontal) ? BLI_rctf_size_x(&but->rect) :
                                         -BLI_rctf_size_y(&but->rect);
    cursor_x_range = size * (but->softmax - but->softmin) /
                     (but->softmax - but->softmin + scroll_but->visual_height);
  }
  else {
    const float ofs = (BLI_rctf_size_y(&but->rect) / 2.0f);
    cursor_x_range = (BLI_rctf_size_x(&but->rect) - ofs);
  }

  f = (mx_fl - data->dragstartx) / cursor_x_range + data->dragfstart;
  CLAMP(f, 0.0f, 1.0f);

  /* deal with mouse correction */
#ifdef USE_CONT_MOUSE_CORRECT
  if (ui_but_is_cursor_warp(but)) {
    /* OK but can go outside bounds */
    if (is_horizontal) {
      data->ungrab_mval[0] = but->rect.xmin + (f * cursor_x_range);
      data->ungrab_mval[1] = BLI_rctf_cent_y(&but->rect);
    }
    else {
      data->ungrab_mval[1] = but->rect.ymin + (f * cursor_x_range);
      data->ungrab_mval[0] = BLI_rctf_cent_x(&but->rect);
    }
    BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
  }
#endif
  /* done correcting mouse */

  switch (scale_type) {
    case PROP_SCALE_LINEAR: {
      tempf = softmin + f * softrange;
      break;
    }
    case PROP_SCALE_LOG: {
      tempf = powf(softmax / softmin, f) * softmin;
      break;
    }
    case PROP_SCALE_CUBIC: {
      const float cubicmin = cube_f(softmin);
      const float cubicmax = cube_f(softmax);
      const float cubicrange = cubicmax - cubicmin;
      tempf = cube_f(softmin + f * softrange);
      tempf = (tempf - cubicmin) / cubicrange * softrange + softmin;
      break;
    }
  }
  temp = round_fl_to_int(tempf);

  if (snap) {
    if (ELEM(tempf, softmin, softmax)) {
      /* pass */
    }
    else if (ui_but_is_float(but)) {

      if (shift) {
        if (ELEM(tempf, softmin, softmax)) {
        }
        else if (softrange < 2.10f) {
          tempf = roundf(tempf * 100.0f) * 0.01f;
        }
        else if (softrange < 21.0f) {
          tempf = roundf(tempf * 10.0f) * 0.1f;
        }
        else {
          tempf = roundf(tempf);
        }
      }
      else {
        if (softrange < 2.10f) {
          tempf = roundf(tempf * 10.0f) * 0.1f;
        }
        else if (softrange < 21.0f) {
          tempf = roundf(tempf);
        }
        else {
          tempf = roundf(tempf * 0.1f) * 10.0f;
        }
      }
    }
    else {
      temp = 10 * (temp / 10);
      tempf = temp;
    }
  }

  if (!ui_but_is_float(but)) {
    lvalue = round(data->value);

    CLAMP(temp, softmin, softmax);

    if (temp != lvalue) {
      data->value = temp;
      data->dragchange = true;
      changed = true;
    }
  }
  else {
    CLAMP(tempf, softmin, softmax);

    if (tempf != float(data->value)) {
      data->value = tempf;
      data->dragchange = true;
      changed = true;
    }
  }

  return changed;
}

static int ui_do_but_SLI(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int click = 0;
  int retval = WM_UI_HANDLER_CONTINUE;

  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  /* Defer evaluation as it's rarely needed. */
  auto drag_threshold_fn = [event]() -> int { return WM_event_drag_threshold(event); };

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    int type = event->type, val = event->val;

    if (type == MOUSEPAN) {
      ui_pan_to_scroll(event, &type, &val);
    }

    /* XXX hardcoded keymap check.... */
    if ((type == MOUSEPAN) && (event->modifier & KM_CTRL)) {
      /* allow accumulating values, otherwise scrolling gets preference */
      retval = WM_UI_HANDLER_BREAK;
    }
    else if ((type == WHEELDOWNMOUSE) && (event->modifier & KM_CTRL)) {
      mx = but->rect.xmin;
      click = 2;
    }
    else if ((type == WHEELUPMOUSE) && (event->modifier & KM_CTRL)) {
      mx = but->rect.xmax;
      click = 2;
    }
    else if (event->val == KM_PRESS) {
      if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && (event->modifier & KM_CTRL)) {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
#ifndef USE_ALLSELECT
      /* alt-click on sides to get "arrows" like in ButType::Num buttons,
       * and match wheel usage above */
      else if ((event->type == LEFTMOUSE) && (event->modifier & KM_ALT)) {
        int halfpos = BLI_rctf_cent_x(&but->rect);
        click = 2;
        if (mx < halfpos) {
          mx = but->rect.xmin;
        }
        else {
          mx = but->rect.xmax;
        }
      }
#endif
      else if (event->type == LEFTMOUSE) {
        data->dragstartx = mx;
        data->draglastx = mx;
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
      else if (ELEM(event->type, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
        click = 1;
      }
      else if (event->type == EVT_MINUSKEY && event->val == KM_PRESS) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        data->value = -data->value;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
      }
    }
#ifdef USE_DRAG_MULTINUM
    copy_v2_v2_int(data->multi_data.drag_start, event->xy);
#endif
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (data->dragchange) {
#ifdef USE_DRAG_MULTINUM
        /* If we started multi-button but didn't drag, then edit. */
        if (data->multi_data.init == uiHandleButtonMulti::INIT_SETUP) {
          click = 1;
        }
        else
#endif
        {
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
      }
      else {
#ifdef USE_CONT_MOUSE_CORRECT
        /* reset! */
        copy_v2_fl(data->ungrab_mval, FLT_MAX);
#endif
        click = 1;
      }
    }
    else if ((event->type == MOUSEMOVE) || ui_event_is_snap(event)) {
      const bool is_motion = (event->type == MOUSEMOVE);
#ifdef USE_DRAG_MULTINUM
      data->multi_data.drag_dir[0] += abs(data->draglastx - mx);
      data->multi_data.drag_dir[1] += abs(data->draglasty - my);
#endif
      if (ui_numedit_but_SLI(but,
                             data,
                             mx,
                             drag_threshold_fn,
                             true,
                             is_motion,
                             event->modifier & KM_CTRL,
                             event->modifier & KM_SHIFT))
      {
        ui_numedit_apply(C, block, but, data);
      }

#ifdef USE_DRAG_MULTINUM
      else if (data->multi_data.has_mbuts) {
        if (data->multi_data.init == uiHandleButtonMulti::INIT_ENABLE) {
          ui_multibut_states_apply(C, data, block);
        }
      }
#endif
    }
    retval = WM_UI_HANDLER_BREAK;
  }
  else if (data->state == BUTTON_STATE_TEXT_EDITING) {
    ui_do_but_textedit(C, block, but, data, event);
    retval = WM_UI_HANDLER_BREAK;
  }
  else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
    ui_do_but_textedit_select(C, block, but, data, event);
    retval = WM_UI_HANDLER_BREAK;
  }

  if (click) {
    if (click == 2) {
      const PropertyScaleType scale_type = ui_but_scale_type(but);

      /* nudge slider to the left or right */
      float f, tempf, softmin, softmax, softrange;
      int temp;

      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      softmin = but->softmin;
      softmax = but->softmax;
      softrange = softmax - softmin;

      tempf = data->value;
      temp = int(data->value);

#if 0
      if (but->type == SLI) {
        /* same as below */
        f = float(mx - but->rect.xmin) / (BLI_rctf_size_x(&but->rect));
      }
      else
#endif
      {
        f = float(mx - but->rect.xmin) / BLI_rctf_size_x(&but->rect);
      }

      if (scale_type == PROP_SCALE_LOG) {
        f = powf(softmax / softmin, f) * softmin;
      }
      else {
        f = softmin + f * softrange;
      }

      if (!ui_but_is_float(but)) {
        int value_step = 1;
        if (f < temp) {
          temp -= value_step;
        }
        else {
          temp += value_step;
        }

        if (temp >= softmin && temp <= softmax) {
          data->value = temp;
        }
        else {
          data->cancel = true;
        }
      }
      else {
        if (tempf >= softmin && tempf <= softmax) {
          float value_step;
          if (scale_type == PROP_SCALE_LOG) {
            value_step = powf(10.0f, roundf(log10f(tempf) + UI_PROP_SCALE_LOG_SNAP_OFFSET) - 1.0f);
          }
          else {
            value_step = 0.01f;
          }

          if (f < tempf) {
            tempf -= value_step;
          }
          else {
            tempf += value_step;
          }

          CLAMP(tempf, softmin, softmax);
          data->value = tempf;
        }
        else {
          data->cancel = true;
        }
      }

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      retval = WM_UI_HANDLER_BREAK;
    }
    else {
      /* edit the value directly */
      button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      retval = WM_UI_HANDLER_BREAK;
    }
  }

  data->draglastx = mx;
  data->draglasty = my;

  return retval;
}

static int ui_do_but_SCROLL(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int retval = WM_UI_HANDLER_CONTINUE;
  const bool horizontal = (BLI_rctf_size_x(&but->rect) > BLI_rctf_size_y(&but->rect));

  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  /* Defer evaluation as it's rarely needed. */
  auto drag_threshold_fn = [event]() -> int { return WM_event_drag_threshold(event); };

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->val == KM_PRESS) {
      if (event->type == LEFTMOUSE) {
        if (horizontal) {
          data->dragstartx = mx;
          data->draglastx = mx;
        }
        else {
          data->dragstartx = my;
          data->draglastx = my;
        }
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == EVT_ESCKEY) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    else if (event->type == MOUSEMOVE) {
      const bool is_motion = true;
      if (ui_numedit_but_SLI(but,
                             data,
                             (horizontal) ? mx : my,
                             drag_threshold_fn,
                             horizontal,
                             is_motion,
                             false,
                             false))
      {
        /* Scroll-bars in popups need UI layout refresh to update the right items to show. */
        if (ui_block_is_popup_any(but->block)) {
          ED_region_tag_refresh_ui(data->region);
        }
        ui_numedit_apply(C, block, but, data);
      }
    }

    retval = WM_UI_HANDLER_BREAK;
  }

  return retval;
}

static int ui_do_but_GRIP(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int retval = WM_UI_HANDLER_CONTINUE;
  const bool horizontal = (BLI_rctf_size_x(&but->rect) < BLI_rctf_size_y(&but->rect));

  /* NOTE: Having to store org point in window space and recompute it to block "space" each time
   *       is not ideal, but this is a way to hack around behavior of ui_window_to_block(), which
   *       returns different results when the block is inside a panel or not...
   *       See #37739.
   */

  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->val == KM_PRESS) {
      if (event->type == LEFTMOUSE) {
        data->dragstartx = event->xy[0];
        data->dragstarty = event->xy[1];
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == EVT_ESCKEY) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    else if (event->type == MOUSEMOVE) {
      int dragstartx = data->dragstartx;
      int dragstarty = data->dragstarty;
      ui_window_to_block(data->region, block, &dragstartx, &dragstarty);
      data->value = data->origvalue + (horizontal ? mx - dragstartx : dragstarty - my);
      ui_numedit_apply(C, block, but, data);
    }

    retval = WM_UI_HANDLER_BREAK;
  }

  return retval;
}

static int ui_do_but_LISTROW(bContext *C,
                             uiBut *but,
                             uiHandleButtonData *data,
                             const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    /* hack to pass on ctrl+click and double click to overlapping text
     * editing field for editing list item names
     */
    if ((ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && (event->val == KM_PRESS) &&
         (event->modifier & KM_CTRL)) ||
        (event->type == LEFTMOUSE && event->val == KM_DBL_CLICK))
    {
      uiBut *labelbut = ui_but_list_row_text_activate(
          C, but, data, event, BUTTON_ACTIVATE_TEXT_EDITING);
      if (labelbut) {
        /* Nothing else to do. */
        return WM_UI_HANDLER_BREAK;
      }
    }
  }

  return ui_do_but_EXIT(C, but, data, event);
}

static int ui_do_but_BLOCK(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {

    /* First handle click on icon-drag type button. */
    if (event->type == LEFTMOUSE && ui_but_drag_is_draggable(but) && event->val == KM_PRESS) {
      if (ui_but_contains_point_px_icon(but, data->region, event)) {
        button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
        data->dragstartx = event->xy[0];
        data->dragstarty = event->xy[1];
        return WM_UI_HANDLER_BREAK;
      }
    }
#ifdef USE_DRAG_TOGGLE
    if (event->type == LEFTMOUSE && event->val == KM_PRESS && ui_but_is_drag_toggle(but)) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->xy[0];
      data->dragstarty = event->xy[1];
      return WM_UI_HANDLER_BREAK;
    }
#endif
    /* regular open menu */
    if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
      button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
      return WM_UI_HANDLER_BREAK;
    }
    if (ui_but_supports_cycling(but)) {
      if (ELEM(event->type, MOUSEPAN, WHEELDOWNMOUSE, WHEELUPMOUSE) && (event->modifier & KM_CTRL))
      {
        int type = event->type;
        int val = event->val;

        /* Convert pan to scroll-wheel. */
        if (type == MOUSEPAN) {
          ui_pan_to_scroll(event, &type, &val);

          if (type == MOUSEPAN) {
            return WM_UI_HANDLER_BREAK;
          }
        }

        const int direction = (type == WHEELDOWNMOUSE) ? 1 : -1;

        data->value = ui_but_menu_step(but, direction);

        button_activate_state(C, but, BUTTON_STATE_EXIT);
        ui_apply_but(C, but->block, but, data, true);

        /* Button's state need to be changed to EXIT so moving mouse away from this mouse
         * wouldn't lead to cancel changes made to this button, but changing state to EXIT also
         * makes no button active for a while which leads to triggering operator when doing fast
         * scrolling mouse wheel. using post activate stuff from button allows to make button be
         * active again after checking for all that mouse leave and cancel stuff, so quick
         * scroll wouldn't be an issue anymore. Same goes for scrolling wheel in another
         * direction below (sergey).
         */
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_OVER;

        /* without this, a new interface that draws as result of the menu change
         * won't register that the mouse is over it, eg:
         * Alt+MouseWheel over the render slots, without this,
         * the slot menu fails to switch a second time.
         *
         * The active state of the button could be maintained some other way
         * and remove this mouse-move event.
         */
        WM_event_add_mousemove(data->window);

        return WM_UI_HANDLER_BREAK;
      }
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_DRAG) {

    /* this function also ends state */
    if (ui_but_drag_init(C, but, data, event)) {
      return WM_UI_HANDLER_BREAK;
    }

    /* outside icon quit, not needed if drag activated */
    if (0 == ui_but_contains_point_px_icon(but, data->region, event)) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      data->cancel = true;
      return WM_UI_HANDLER_BREAK;
    }

    if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
      return WM_UI_HANDLER_BREAK;
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_UNITVEC(
    uiBut *but, uiHandleButtonData *data, int mx, int my, const enum eSnapType snap)
{
  float mrad;
  bool changed = true;

  /* button is presumed square */
  /* if mouse moves outside of sphere, it does negative normal */

  /* note that both data->vec and data->origvec should be normalized
   * else we'll get a harmless but annoying jump when first clicking */

  float *fp = data->origvec;
  const float rad = BLI_rctf_size_x(&but->rect);
  const float radsq = rad * rad;

  int mdx, mdy;
  if (fp[2] > 0.0f) {
    mdx = (rad * fp[0]);
    mdy = (rad * fp[1]);
  }
  else if (fp[2] > -1.0f) {
    mrad = rad / sqrtf(fp[0] * fp[0] + fp[1] * fp[1]);

    mdx = 2.0f * mrad * fp[0] - (rad * fp[0]);
    mdy = 2.0f * mrad * fp[1] - (rad * fp[1]);
  }
  else {
    mdx = mdy = 0;
  }

  float dx = float(mx + mdx - data->dragstartx);
  float dy = float(my + mdy - data->dragstarty);

  fp = data->vec;
  mrad = dx * dx + dy * dy;
  if (mrad < radsq) { /* inner circle */
    fp[0] = dx;
    fp[1] = dy;
    fp[2] = sqrtf(radsq - dx * dx - dy * dy);
  }
  else { /* outer circle */

    mrad = rad / sqrtf(mrad); /* veclen */

    dx *= (2.0f * mrad - 1.0f);
    dy *= (2.0f * mrad - 1.0f);

    mrad = dx * dx + dy * dy;
    if (mrad < radsq) {
      fp[0] = dx;
      fp[1] = dy;
      fp[2] = -sqrtf(radsq - dx * dx - dy * dy);
    }
  }
  normalize_v3(fp);

  if (snap != SNAP_OFF) {
    const int snap_steps = (snap == SNAP_ON) ? 4 : 12; /* 45 or 15 degree increments */
    const float snap_steps_angle = M_PI / snap_steps;
    float angle, angle_snap;

    /* round each axis of 'fp' to the next increment
     * do this in "angle" space - this gives increments of same size */
    for (int i = 0; i < 3; i++) {
      angle = asinf(fp[i]);
      angle_snap = roundf(angle / snap_steps_angle) * snap_steps_angle;
      fp[i] = sinf(angle_snap);
    }
    normalize_v3(fp);
    changed = !compare_v3v3(fp, data->origvec, FLT_EPSILON);
  }

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static void ui_palette_set_active(uiButColor *color_but)
{
  if (color_but->is_pallete_color) {
    Palette *palette = (Palette *)color_but->rnapoin.owner_id;
    const PaletteColor *color = static_cast<const PaletteColor *>(color_but->rnapoin.data);
    palette->active_color = BLI_findindex(&palette->colors, color);
  }
}

static int ui_do_but_COLOR(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  BLI_assert(but->type == ButType::Color);
  uiButColor *color_but = (uiButColor *)but;

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    /* First handle click on icon-drag type button. */
    if (event->type == LEFTMOUSE && ui_but_drag_is_draggable(but) && event->val == KM_PRESS) {
      ui_palette_set_active(color_but);
      if (ui_but_contains_point_px_icon(but, data->region, event)) {
        button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
        data->dragstartx = event->xy[0];
        data->dragstarty = event->xy[1];
        return WM_UI_HANDLER_BREAK;
      }
    }
#ifdef USE_DRAG_TOGGLE
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      ui_palette_set_active(color_but);
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->xy[0];
      data->dragstarty = event->xy[1];
      return WM_UI_HANDLER_BREAK;
    }
#endif
    /* regular open menu */
    if (ELEM(event->type, LEFTMOUSE, EVT_PADENTER, EVT_RETKEY) && event->val == KM_PRESS) {
      ui_palette_set_active(color_but);
      button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
      return WM_UI_HANDLER_BREAK;
    }
    if (ELEM(event->type, MOUSEPAN, WHEELDOWNMOUSE, WHEELUPMOUSE) && (event->modifier & KM_CTRL)) {
      ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
      float hsv_static[3] = {0.0f};
      float *hsv = cpicker ? cpicker->hsv_perceptual : hsv_static;
      float col[3];

      ui_but_v3_get(but, col);
      rgb_to_hsv_compat_v(col, hsv);

      if (event->type == WHEELDOWNMOUSE) {
        hsv[2] = clamp_f(hsv[2] - 0.05f, 0.0f, 1.0f);
      }
      else if (event->type == WHEELUPMOUSE) {
        hsv[2] = clamp_f(hsv[2] + 0.05f, 0.0f, 1.0f);
      }
      else {
        const float fac = 0.005 * (event->xy[1] - event->prev_xy[1]);
        hsv[2] = clamp_f(hsv[2] + fac, 0.0f, 1.0f);
      }

      hsv_to_rgb_v(hsv, data->vec);
      ui_but_v3_set(but, data->vec);

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      ui_apply_but(C, but->block, but, data, true);
      return WM_UI_HANDLER_BREAK;
    }
    if (color_but->is_pallete_color && (event->type == EVT_DELKEY) && (event->val == KM_PRESS)) {
      Palette *palette = (Palette *)but->rnapoin.owner_id;
      PaletteColor *color = static_cast<PaletteColor *>(but->rnapoin.data);

      BKE_palette_color_remove(palette, color);

      button_activate_state(C, but, BUTTON_STATE_EXIT);

      /* this is risky. it works OK for now,
       * but if it gives trouble we should delay execution */
      but->rnapoin = PointerRNA_NULL;
      but->rnaprop = nullptr;

      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_DRAG) {

    /* this function also ends state */
    if (ui_but_drag_init(C, but, data, event)) {
      return WM_UI_HANDLER_BREAK;
    }

    /* outside icon quit, not needed if drag activated */
    if (0 == ui_but_contains_point_px_icon(but, data->region, event)) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      data->cancel = true;
      return WM_UI_HANDLER_BREAK;
    }

    if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (color_but->is_pallete_color) {
        if ((event->modifier & KM_CTRL) == 0) {
          float color[3];
          Paint *paint = BKE_paint_get_active_from_context(C);
          if (paint != nullptr) {
            Brush *brush = BKE_paint_brush(paint);

            if (brush == nullptr) {
              /* Pass. */
            }
            else if (brush->flag & BRUSH_USE_GRADIENT) {
              float *target = &brush->gradient->data[brush->gradient->cur].r;

              if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
                RNA_property_float_get_array_at_most(&but->rnapoin, but->rnaprop, target, 3);
                IMB_colormanagement_srgb_to_scene_linear_v3(target, target);
              }
              else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
                RNA_property_float_get_array_at_most(&but->rnapoin, but->rnaprop, target, 3);
              }
              BKE_brush_tag_unsaved_changes(brush);
            }
            else {
              bool updated = false;

              if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
                RNA_property_float_get_array_at_most(
                    &but->rnapoin, but->rnaprop, color, ARRAY_SIZE(color));
                IMB_colormanagement_srgb_to_scene_linear_v3(color, color);
                BKE_brush_color_set(paint, brush, color);
                updated = true;
              }
              else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
                RNA_property_float_get_array_at_most(
                    &but->rnapoin, but->rnaprop, color, ARRAY_SIZE(color));
                BKE_brush_color_set(paint, brush, color);
                updated = true;
              }

              if (updated) {
                PropertyRNA *brush_color_prop;

                PointerRNA brush_ptr = RNA_id_pointer_create(&brush->id);
                brush_color_prop = RNA_struct_find_property(&brush_ptr, "color");
                RNA_property_update(C, &brush_ptr, brush_color_prop);
              }
            }
          }

          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        else {
          button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
        }
      }
      else {
        button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
      }
      return WM_UI_HANDLER_BREAK;
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_UNITVEC(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      const enum eSnapType snap = ui_event_to_snap(event);
      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_UNITVEC(but, data, mx, my, snap)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if ((event->type == MOUSEMOVE) || ui_event_is_snap(event)) {
      if (mx != data->draglastx || my != data->draglasty || event->type != MOUSEMOVE) {
        const enum eSnapType snap = ui_event_to_snap(event);
        if (ui_numedit_but_UNITVEC(but, data, mx, my, snap)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }

    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

/* scales a vector so no axis exceeds max
 * (could become BLI_math func) */
static void clamp_axis_max_v3(float v[3], const float max)
{
  const float v_max = max_fff(v[0], v[1], v[2]);
  if (v_max > max) {
    mul_v3_fl(v, max / v_max);
    v[0] = std::min(v[0], max);
    v[1] = std::min(v[1], max);
    v[2] = std::min(v[2], max);
  }
}

static void ui_rgb_to_color_picker_HSVCUBE_compat_v(const uiButHSVCube *hsv_but,
                                                    const float rgb[3],
                                                    float hsv[3])
{
  if (hsv_but->gradient_type == UI_GRAD_L_ALT) {
    rgb_to_hsl_compat_v(rgb, hsv);
  }
  else {
    rgb_to_hsv_compat_v(rgb, hsv);
  }
}

static void ui_rgb_to_color_picker_HSVCUBE_v(const uiButHSVCube *hsv_but,
                                             const float rgb[3],
                                             float hsv[3])
{
  if (hsv_but->gradient_type == UI_GRAD_L_ALT) {
    rgb_to_hsl_v(rgb, hsv);
  }
  else {
    rgb_to_hsv_v(rgb, hsv);
  }
}

static void ui_color_picker_to_rgb_HSVCUBE_v(const uiButHSVCube *hsv_but,
                                             const float hsv[3],
                                             float rgb[3])
{
  if (hsv_but->gradient_type == UI_GRAD_L_ALT) {
    hsl_to_rgb_v(hsv, rgb);
  }
  else {
    hsv_to_rgb_v(hsv, rgb);
  }
}

static bool ui_numedit_but_HSVCUBE(uiBut *but,
                                   uiHandleButtonData *data,
                                   int mx,
                                   int my,
                                   const enum eSnapType snap,
                                   const bool shift)
{
  const uiButHSVCube *hsv_but = (uiButHSVCube *)but;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  float *hsv = cpicker->hsv_perceptual;
  float rgb[3];
  float x, y;
  float mx_fl, my_fl;
  const bool changed = true;

  ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);

#ifdef USE_CONT_MOUSE_CORRECT
  if (ui_but_is_cursor_warp(but)) {
    /* OK but can go outside bounds */
    data->ungrab_mval[0] = mx_fl;
    data->ungrab_mval[1] = my_fl;
    BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
  }
#endif

  /* Always start from original value to avoid numerical drift. */
  copy_v3_v3(rgb, data->origvec);
  ui_scene_linear_to_perceptual_space(but, rgb);

  ui_rgb_to_color_picker_HSVCUBE_compat_v(hsv_but, rgb, hsv);

  /* only apply the delta motion, not absolute */
  if (shift) {
    rcti rect_i;
    float xpos, ypos, hsvo[3];

    BLI_rcti_rctf_copy(&rect_i, &but->rect);

    /* calculate original hsv again */
    copy_v3_v3(rgb, data->origvec);
    ui_scene_linear_to_perceptual_space(but, rgb);

    copy_v3_v3(hsvo, hsv);

    ui_rgb_to_color_picker_HSVCUBE_compat_v(hsv_but, rgb, hsvo);

    /* and original position */
    ui_hsvcube_pos_from_vals(hsv_but, &rect_i, hsvo, &xpos, &ypos);

    mx_fl = xpos - (data->dragstartx - mx_fl);
    my_fl = ypos - (data->dragstarty - my_fl);
  }

  /* relative position within box */
  x = (mx_fl - but->rect.xmin) / BLI_rctf_size_x(&but->rect);
  y = (my_fl - but->rect.ymin) / BLI_rctf_size_y(&but->rect);
  CLAMP(x, 0.0f, 1.0f);
  CLAMP(y, 0.0f, 1.0f);

  switch (hsv_but->gradient_type) {
    case UI_GRAD_SV:
      hsv[1] = x;
      hsv[2] = y;
      break;
    case UI_GRAD_HV:
      hsv[0] = x;
      hsv[2] = y;
      break;
    case UI_GRAD_HS:
      hsv[0] = x;
      hsv[1] = y;
      break;
    case UI_GRAD_H:
      hsv[0] = x;
      break;
    case UI_GRAD_S:
      hsv[1] = x;
      break;
    case UI_GRAD_V:
      hsv[2] = x;
      break;
    case UI_GRAD_L_ALT:
      hsv[2] = y;
      break;
    case UI_GRAD_V_ALT: {
      /* vertical 'value' strip */
      const float min = but->softmin, max = but->softmax;
      /* exception only for value strip - use the range set in but->min/max */
      hsv[2] = y * (max - min) + min;
      break;
    }
    default:
      BLI_assert(0);
      break;
  }

  if (snap != SNAP_OFF) {
    if (ELEM(hsv_but->gradient_type, UI_GRAD_HV, UI_GRAD_HS, UI_GRAD_H)) {
      ui_color_snap_hue(snap, &hsv[0]);
    }
  }

  ui_color_picker_to_rgb_HSVCUBE_v(hsv_but, hsv, rgb);
  ui_perceptual_to_scene_linear_space(but, rgb);

  /* clamp because with color conversion we can exceed range #34295. */
  if (hsv_but->gradient_type == UI_GRAD_V_ALT) {
    clamp_axis_max_v3(rgb, but->softmax);
  }

  copy_v3_v3(data->vec, rgb);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

#ifdef WITH_INPUT_NDOF
static void ui_ndofedit_but_HSVCUBE(uiButHSVCube *hsv_but,
                                    uiHandleButtonData *data,
                                    const wmNDOFMotionData &ndof,
                                    const enum eSnapType snap,
                                    const bool shift)
{
  ColorPicker *cpicker = static_cast<ColorPicker *>(hsv_but->custom_data);
  float *hsv = cpicker->hsv_perceptual;
  const float hsv_v_max = max_ff(hsv[2], hsv_but->softmax);
  float rgb[3];
  const float sensitivity = (shift ? 0.15f : 0.3f) * ndof.time_delta;

  ui_but_v3_get(hsv_but, rgb);
  ui_scene_linear_to_perceptual_space(hsv_but, rgb);
  ui_rgb_to_color_picker_HSVCUBE_compat_v(hsv_but, rgb, hsv);

  switch (hsv_but->gradient_type) {
    case UI_GRAD_SV:
      hsv[1] += ndof.rvec[2] * sensitivity;
      hsv[2] += ndof.rvec[0] * sensitivity;
      break;
    case UI_GRAD_HV:
      hsv[0] += ndof.rvec[2] * sensitivity;
      hsv[2] += ndof.rvec[0] * sensitivity;
      break;
    case UI_GRAD_HS:
      hsv[0] += ndof.rvec[2] * sensitivity;
      hsv[1] += ndof.rvec[0] * sensitivity;
      break;
    case UI_GRAD_H:
      hsv[0] += ndof.rvec[2] * sensitivity;
      break;
    case UI_GRAD_S:
      hsv[1] += ndof.rvec[2] * sensitivity;
      break;
    case UI_GRAD_V:
      hsv[2] += ndof.rvec[2] * sensitivity;
      break;
    case UI_GRAD_V_ALT:
    case UI_GRAD_L_ALT:
      /* vertical 'value' strip */

      /* exception only for value strip - use the range set in but->min/max */
      hsv[2] += ndof.rvec[0] * sensitivity;

      CLAMP(hsv[2], hsv_but->softmin, hsv_but->softmax);
      break;
    default:
      BLI_assert_msg(0, "invalid hsv type");
      break;
  }

  if (snap != SNAP_OFF) {
    if (ELEM(hsv_but->gradient_type, UI_GRAD_HV, UI_GRAD_HS, UI_GRAD_H)) {
      ui_color_snap_hue(snap, &hsv[0]);
    }
  }

  /* ndof specific: the changes above aren't clamping */
  hsv_clamp_v(hsv, hsv_v_max);

  ui_color_picker_to_rgb_HSVCUBE_v(hsv_but, hsv, rgb);
  ui_perceptual_to_scene_linear_space(hsv_but, rgb);

  copy_v3_v3(data->vec, rgb);
  ui_but_v3_set(hsv_but, data->vec);
}
#endif /* WITH_INPUT_NDOF */

static int ui_do_but_HSVCUBE(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  uiButHSVCube *hsv_but = (uiButHSVCube *)but;
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      const enum eSnapType snap = ui_event_to_snap(event);

      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_HSVCUBE(but, data, mx, my, snap, event->modifier & KM_SHIFT)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
#ifdef WITH_INPUT_NDOF
    if (event->type == NDOF_MOTION) {
      const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);
      const enum eSnapType snap = ui_event_to_snap(event);

      ui_ndofedit_but_HSVCUBE(hsv_but, data, ndof, snap, event->modifier & KM_SHIFT);

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      ui_apply_but(C, but->block, but, data, true);

      return WM_UI_HANDLER_BREAK;
    }
#endif /* WITH_INPUT_NDOF */
    /* XXX hardcoded keymap check.... */
    if (event->type == EVT_BACKSPACEKEY && event->val == KM_PRESS) {
      if (ELEM(hsv_but->gradient_type, UI_GRAD_V_ALT, UI_GRAD_L_ALT)) {
        int len;

        /* reset only value */

        len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
        if (ELEM(len, 3, 4)) {
          float rgb[3], def_hsv[3];
          float def[4];
          ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
          float *hsv = cpicker->hsv_perceptual;

          RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
          ui_rgb_to_color_picker_HSVCUBE_v(hsv_but, def, def_hsv);

          ui_but_v3_get(but, rgb);
          ui_rgb_to_color_picker_HSVCUBE_compat_v(hsv_but, rgb, hsv);

          def_hsv[0] = hsv[0];
          def_hsv[1] = hsv[1];

          ui_color_picker_to_rgb_HSVCUBE_v(hsv_but, def_hsv, rgb);
          ui_but_v3_set(but, rgb);
          ui_apply_but_func(C, but);

          RNA_property_update(C, &but->rnapoin, but->rnaprop);
          return WM_UI_HANDLER_BREAK;
        }
      }
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if ((event->type == MOUSEMOVE) || ui_event_is_snap(event)) {
      if (mx != data->draglastx || my != data->draglasty || event->type != MOUSEMOVE) {
        const enum eSnapType snap = ui_event_to_snap(event);

        if (ui_numedit_but_HSVCUBE(but, data, mx, my, snap, event->modifier & KM_SHIFT)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }

    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_HSVCIRCLE(uiBut *but,
                                     uiHandleButtonData *data,
                                     float mx,
                                     float my,
                                     const enum eSnapType snap,
                                     const bool shift)
{
  const bool changed = true;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  float *hsv = cpicker->hsv_perceptual;

  float mx_fl, my_fl;
  ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);

#ifdef USE_CONT_MOUSE_CORRECT
  if (ui_but_is_cursor_warp(but)) {
    /* OK but can go outside bounds */
    data->ungrab_mval[0] = mx_fl;
    data->ungrab_mval[1] = my_fl;
    { /* clamp */
      const float radius = min_ff(BLI_rctf_size_x(&but->rect), BLI_rctf_size_y(&but->rect)) / 2.0f;
      const float cent[2] = {BLI_rctf_cent_x(&but->rect), BLI_rctf_cent_y(&but->rect)};
      const float len = len_v2v2(cent, data->ungrab_mval);
      if (len > radius) {
        dist_ensure_v2_v2fl(data->ungrab_mval, cent, radius);
      }
    }
  }
#endif

  rcti rect;
  BLI_rcti_rctf_copy(&rect, &but->rect);

  /* Always start from original value to avoid numerical drift. */
  float rgb[3];
  copy_v3_v3(rgb, data->origvec);
  ui_scene_linear_to_perceptual_space(but, rgb);
  ui_color_picker_rgb_to_hsv_compat(rgb, hsv);

  /* exception, when using color wheel in 'locked' value state:
   * allow choosing a hue for black values, by giving a tiny increment */
  if (cpicker->use_color_lock) {
    if (U.color_picker_type == USER_CP_CIRCLE_HSV) { /* lock */
      if (hsv[2] == 0.0f) {
        hsv[2] = 0.0001f;
      }
    }
    else {
      if (hsv[2] == 0.0f) {
        hsv[2] = 0.0001f;
      }
      hsv[2] = std::min(hsv[2], 0.9999f);
    }
  }

  /* only apply the delta motion, not absolute */
  if (shift) {
    float xpos, ypos, hsvo[3], rgbo[3];

    /* calculate original hsv again */
    copy_v3_v3(hsvo, hsv);
    copy_v3_v3(rgbo, data->origvec);
    ui_scene_linear_to_perceptual_space(but, rgbo);
    ui_color_picker_rgb_to_hsv_compat(rgbo, hsvo);

    /* and original position */
    ui_hsvcircle_pos_from_vals(cpicker, &rect, hsvo, &xpos, &ypos);

    mx_fl = xpos - (data->dragstartx - mx_fl);
    my_fl = ypos - (data->dragstarty - my_fl);
  }

  ui_hsvcircle_vals_from_pos(&rect, mx_fl, my_fl, hsv, hsv + 1);

  if ((cpicker->use_color_cubic) && (U.color_picker_type == USER_CP_CIRCLE_HSV)) {
    hsv[1] = 1.0f - sqrt3f(1.0f - hsv[1]);
  }

  if (snap != SNAP_OFF) {
    ui_color_snap_hue(snap, &hsv[0]);
  }

  ui_color_picker_hsv_to_rgb(hsv, rgb);

  if (cpicker->use_luminosity_lock) {
    if (!is_zero_v3(rgb)) {
      normalize_v3_length(rgb, cpicker->luminosity_lock_value);
    }
  }

  ui_perceptual_to_scene_linear_space(but, rgb);
  ui_but_v3_set(but, rgb);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

#ifdef WITH_INPUT_NDOF
static void ui_ndofedit_but_HSVCIRCLE(uiBut *but,
                                      uiHandleButtonData *data,
                                      const wmNDOFMotionData &ndof,
                                      const enum eSnapType snap,
                                      const bool shift)
{
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  float *hsv = cpicker->hsv_perceptual;
  float rgb[3];
  float phi, r, v[2];
  const float sensitivity = (shift ? 0.06f : 0.3f) * ndof.time_delta;

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_perceptual_space(but, rgb);
  ui_color_picker_rgb_to_hsv_compat(rgb, hsv);

  /* Convert current color on hue/sat disc to circular coordinates phi, r */
  phi = fmodf(hsv[0] + 0.25f, 1.0f) * -2.0f * float(M_PI);
  r = hsv[1];
  // const float sqr = r > 0.0f ? sqrtf(r) : 1; /* UNUSED */

  /* Convert to 2d vectors */
  v[0] = r * cosf(phi);
  v[1] = r * sinf(phi);

  /* Use ndof device y and x rotation to move the vector in 2d space */
  v[0] += ndof.rvec[2] * sensitivity;
  v[1] += ndof.rvec[0] * sensitivity;

  /* convert back to polar coords on circle */
  phi = atan2f(v[0], v[1]) / (2.0f * float(M_PI)) + 0.5f;

  /* use ndof Y rotation to additionally rotate hue */
  phi += ndof.rvec[1] * sensitivity * 0.5f;
  r = len_v2(v);

  /* convert back to hsv values, in range [0,1] */
  hsv[0] = phi;
  hsv[1] = r;

  /* exception, when using color wheel in 'locked' value state:
   * allow choosing a hue for black values, by giving a tiny increment */
  if (cpicker->use_color_lock) {
    if (U.color_picker_type == USER_CP_CIRCLE_HSV) { /* lock */
      if (hsv[2] == 0.0f) {
        hsv[2] = 0.0001f;
      }
    }
    else {
      if (hsv[2] == 0.0f) {
        hsv[2] = 0.0001f;
      }
      if (hsv[2] == 1.0f) {
        hsv[2] = 0.9999f;
      }
    }
  }

  if (snap != SNAP_OFF) {
    ui_color_snap_hue(snap, &hsv[0]);
  }

  hsv_clamp_v(hsv, FLT_MAX);

  ui_color_picker_hsv_to_rgb(hsv, data->vec);

  if (cpicker->use_luminosity_lock) {
    if (!is_zero_v3(data->vec)) {
      normalize_v3_length(data->vec, cpicker->luminosity_lock_value);
    }
  }

  ui_perceptual_to_scene_linear_space(but, data->vec);
  ui_but_v3_set(but, data->vec);
}
#endif /* WITH_INPUT_NDOF */

static int ui_do_but_HSVCIRCLE(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  float *hsv = cpicker->hsv_perceptual;
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      const enum eSnapType snap = ui_event_to_snap(event);
      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, snap, event->modifier & KM_SHIFT)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
#ifdef WITH_INPUT_NDOF
    if (event->type == NDOF_MOTION) {
      const enum eSnapType snap = ui_event_to_snap(event);
      const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);

      ui_ndofedit_but_HSVCIRCLE(but, data, ndof, snap, event->modifier & KM_SHIFT);

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      ui_apply_but(C, but->block, but, data, true);

      return WM_UI_HANDLER_BREAK;
    }
#endif /* WITH_INPUT_NDOF */
    /* XXX hardcoded keymap check.... */
    if (event->type == EVT_BACKSPACEKEY && event->val == KM_PRESS) {
      int len;

      /* reset only saturation */

      len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
      if (len >= 3) {
        float rgb[3], def_hsv[3];
        float *def = MEM_calloc_arrayN<float>(len, __func__);

        RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
        ui_color_picker_hsv_to_rgb(def, def_hsv);

        ui_but_v3_get(but, rgb);
        ui_color_picker_rgb_to_hsv_compat(rgb, hsv);

        def_hsv[0] = hsv[0];
        def_hsv[2] = hsv[2];

        hsv_to_rgb_v(def_hsv, rgb);
        ui_but_v3_set(but, rgb);
        ui_apply_but_func(C, but);

        RNA_property_update(C, &but->rnapoin, but->rnaprop);

        MEM_freeN(def);
      }
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    /* XXX hardcoded keymap check.... */
    else if (event->type == WHEELDOWNMOUSE) {
      hsv[2] = clamp_f(hsv[2] - 0.05f, 0.0f, 1.0f);
      ui_but_hsv_set(but); /* converts to rgb */
      ui_numedit_apply(C, block, but, data);
    }
    else if (event->type == WHEELUPMOUSE) {
      hsv[2] = clamp_f(hsv[2] + 0.05f, 0.0f, 1.0f);
      ui_but_hsv_set(but); /* converts to rgb */
      ui_numedit_apply(C, block, but, data);
    }
    else if ((event->type == MOUSEMOVE) || ui_event_is_snap(event)) {
      if (mx != data->draglastx || my != data->draglasty || event->type != MOUSEMOVE) {
        const enum eSnapType snap = ui_event_to_snap(event);

        if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, snap, event->modifier & KM_SHIFT)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_COLORBAND(uiBut *but, uiHandleButtonData *data, int mx)
{
  bool changed = false;

  if (data->draglastx == mx) {
    return changed;
  }

  if (data->coba->tot == 0) {
    return changed;
  }

  const float dx = float(mx - data->draglastx) / BLI_rctf_size_x(&but->rect);
  data->dragcbd->pos += dx;
  CLAMP(data->dragcbd->pos, 0.0f, 1.0f);

  BKE_colorband_update_sort(data->coba);
  data->dragcbd = data->coba->data + data->coba->cur; /* because qsort */

  data->draglastx = mx;
  changed = true;

  return changed;
}

static int ui_do_but_COLORBAND(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      ColorBand *coba = (ColorBand *)but->poin;

      if (event->modifier & KM_CTRL) {
        /* insert new key on mouse location */
        const float pos = float(mx - but->rect.xmin) / BLI_rctf_size_x(&but->rect);
        BKE_colorband_element_add(coba, pos);
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
      else {
        CBData *cbd;
        /* ignore zoom-level for mindist */
        int mindist = (50 * UI_SCALE_FAC) * block->aspect;
        int xco;
        data->dragstartx = mx;
        data->dragstarty = my;
        data->draglastx = mx;
        data->draglasty = my;

        /* activate new key when mouse is close */
        int a;
        for (a = 0, cbd = coba->data; a < coba->tot; a++, cbd++) {
          xco = but->rect.xmin + (cbd->pos * BLI_rctf_size_x(&but->rect));
          xco = abs(xco - mx);
          if (a == coba->cur) {
            /* Selected one disadvantage. */
            xco += 5;
          }
          if (xco < mindist) {
            coba->cur = a;
            mindist = xco;
          }
        }

        data->dragcbd = coba->data + coba->cur;
        data->dragfstart = data->dragcbd->pos;
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
      }

      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == MOUSEMOVE) {
      if (mx != data->draglastx || my != data->draglasty) {
        if (ui_numedit_but_COLORBAND(but, data, mx)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    else if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
      if (event->val == KM_PRESS) {
        data->dragcbd->pos = data->dragfstart;
        BKE_colorband_update_sort(data->coba);
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_CURVE(uiBlock *block,
                                 uiBut *but,
                                 uiHandleButtonData *data,
                                 int evtx,
                                 int evty,
                                 bool snap,
                                 const bool shift)
{
  CurveMapping *cumap = (CurveMapping *)but->poin;
  CurveMap *cuma = cumap->cm + cumap->cur;
  CurveMapPoint *cmp = cuma->curve;
  bool changed = false;

  /* evtx evty and drag coords are absolute mouse-coords,
   * prevents errors when editing when layout changes. */
  int mx = evtx;
  int my = evty;
  ui_window_to_block(data->region, block, &mx, &my);
  int dragx = data->draglastx;
  int dragy = data->draglasty;
  ui_window_to_block(data->region, block, &dragx, &dragy);

  const float zoomx = BLI_rctf_size_x(&but->rect) / BLI_rctf_size_x(&cumap->curr);
  const float zoomy = BLI_rctf_size_y(&but->rect) / BLI_rctf_size_y(&cumap->curr);

  if (snap) {
    float d[2];

    d[0] = mx - data->dragstartx;
    d[1] = my - data->dragstarty;

    if (len_squared_v2(d) < (3.0f * 3.0f)) {
      snap = false;
    }
  }

  float fx = (mx - dragx) / zoomx;
  float fy = (my - dragy) / zoomy;

  if (data->dragsel != -1) {
    CurveMapPoint *cmp_last = nullptr;
    const float mval_factor = ui_mouse_scale_warp_factor(shift);
    bool moved_point = false; /* for ctrl grid, can't use orig coords because of sorting */

    fx *= mval_factor;
    fy *= mval_factor;

    for (int a = 0; a < cuma->totpoint; a++) {
      if (cmp[a].flag & CUMA_SELECT) {
        const float origx = cmp[a].x, origy = cmp[a].y;
        cmp[a].x += fx;
        cmp[a].y += fy;
        if (snap) {
          cmp[a].x = 0.125f * roundf(8.0f * cmp[a].x);
          cmp[a].y = 0.125f * roundf(8.0f * cmp[a].y);
        }
        if (cmp[a].x != origx || cmp[a].y != origy) {
          moved_point = true;
        }

        cmp_last = &cmp[a];
      }
    }

    BKE_curvemapping_changed(cumap, false);

    if (moved_point) {
      data->draglastx = evtx;
      data->draglasty = evty;
      changed = true;

#ifdef USE_CONT_MOUSE_CORRECT
      /* NOTE: using 'cmp_last' is weak since there may be multiple points selected,
       * but in practice this isn't really an issue */
      if (ui_but_is_cursor_warp(but)) {
        /* OK but can go outside bounds */
        data->ungrab_mval[0] = but->rect.xmin + ((cmp_last->x - cumap->curr.xmin) * zoomx);
        data->ungrab_mval[1] = but->rect.ymin + ((cmp_last->y - cumap->curr.ymin) * zoomy);
        BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
      }
#endif
    }

    data->dragchange = true; /* mark for selection */
  }
  else {
    /* clamp for clip */
    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.xmin - fx < cumap->clipr.xmin) {
        fx = cumap->curr.xmin - cumap->clipr.xmin;
      }
      else if (cumap->curr.xmax - fx > cumap->clipr.xmax) {
        fx = cumap->curr.xmax - cumap->clipr.xmax;
      }
      if (cumap->curr.ymin - fy < cumap->clipr.ymin) {
        fy = cumap->curr.ymin - cumap->clipr.ymin;
      }
      else if (cumap->curr.ymax - fy > cumap->clipr.ymax) {
        fy = cumap->curr.ymax - cumap->clipr.ymax;
      }
    }

    cumap->curr.xmin -= fx;
    cumap->curr.ymin -= fy;
    cumap->curr.xmax -= fx;
    cumap->curr.ymax -= fy;

    data->draglastx = evtx;
    data->draglasty = evty;

    changed = true;
  }

  return changed;
}

static int ui_do_but_CURVE(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  bool changed = false;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      CurveMapping *cumap = (CurveMapping *)but->poin;
      CurveMap *cuma = cumap->cm + cumap->cur;
      const float m_xy[2] = {float(mx), float(my)};
      float dist_min_sq = square_f(UI_SCALE_FAC * 14.0f); /* 14 pixels radius */
      int sel = -1;

      if (event->modifier & KM_CTRL) {
        float f_xy[2];
        BLI_rctf_transform_pt_v(&cumap->curr, &but->rect, f_xy, m_xy);

        BKE_curvemap_insert(cuma, f_xy[0], f_xy[1]);
        BKE_curvemapping_changed(cumap, false);
        changed = true;
      }

      /* check for selecting of a point */
      CurveMapPoint *cmp = cuma->curve; /* ctrl adds point, new malloc */
      for (int a = 0; a < cuma->totpoint; a++) {
        float f_xy[2];
        BLI_rctf_transform_pt_v(&but->rect, &cumap->curr, f_xy, &cmp[a].x);
        const float dist_sq = len_squared_v2v2(m_xy, f_xy);
        if (dist_sq < dist_min_sq) {
          sel = a;
          dist_min_sq = dist_sq;
        }
      }

      if (sel == -1) {
        float f_xy[2], f_xy_prev[2];

        /* if the click didn't select anything, check if it's clicked on the
         * curve itself, and if so, add a point */
        cmp = cuma->table;

        BLI_rctf_transform_pt_v(&but->rect, &cumap->curr, f_xy, &cmp[0].x);

        /* with 160px height 8px should translate to the old 0.05 coefficient at no zoom */
        dist_min_sq = square_f(UI_SCALE_FAC * 8.0f);

        /* loop through the curve segment table and find what's near the mouse. */
        for (int i = 1; i <= CM_TABLE; i++) {
          copy_v2_v2(f_xy_prev, f_xy);
          BLI_rctf_transform_pt_v(&but->rect, &cumap->curr, f_xy, &cmp[i].x);

          if (dist_squared_to_line_segment_v2(m_xy, f_xy_prev, f_xy) < dist_min_sq) {
            BLI_rctf_transform_pt_v(&cumap->curr, &but->rect, f_xy, m_xy);

            BKE_curvemap_insert(cuma, f_xy[0], f_xy[1]);
            BKE_curvemapping_changed(cumap, false);

            changed = true;

            /* reset cmp back to the curve points again,
             * rather than drawing segments */
            cmp = cuma->curve;

            /* find newly added point and make it 'sel' */
            for (int a = 0; a < cuma->totpoint; a++) {
              if (cmp[a].x == f_xy[0]) {
                sel = a;
              }
            }
            break;
          }
        }
      }

      if (sel != -1) {
        /* ok, we move a point */
        /* deselect all if this one is deselect. except if we hold shift */
        if ((event->modifier & KM_SHIFT) == 0) {
          for (int a = 0; a < cuma->totpoint; a++) {
            cmp[a].flag &= ~CUMA_SELECT;
          }
          cmp[sel].flag |= CUMA_SELECT;
        }
        else {
          cmp[sel].flag ^= CUMA_SELECT;
        }
      }
      else {
        /* move the view */
        data->cancel = true;
      }

      data->dragsel = sel;

      data->dragstartx = event->xy[0];
      data->dragstarty = event->xy[1];
      data->draglastx = event->xy[0];
      data->draglasty = event->xy[1];

      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == MOUSEMOVE) {
      if (event->xy[0] != data->draglastx || event->xy[1] != data->draglasty) {

        if (ui_numedit_but_CURVE(block,
                                 but,
                                 data,
                                 event->xy[0],
                                 event->xy[1],
                                 event->modifier & KM_CTRL,
                                 event->modifier & KM_SHIFT))
        {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (data->dragsel != -1) {
        CurveMapping *cumap = (CurveMapping *)but->poin;
        CurveMap *cuma = cumap->cm + cumap->cur;
        CurveMapPoint *cmp = cuma->curve;

        if (data->dragchange == false) {
          /* deselect all, select one */
          if ((event->modifier & KM_SHIFT) == 0) {
            for (int a = 0; a < cuma->totpoint; a++) {
              cmp[a].flag &= ~CUMA_SELECT;
            }
            cmp[data->dragsel].flag |= CUMA_SELECT;
          }
        }
        else {
          BKE_curvemapping_changed(cumap, true); /* remove doubles */
          BKE_paint_invalidate_cursor_overlay(scene, view_layer, cumap);
        }
      }

      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }

    return WM_UI_HANDLER_BREAK;
  }

  /* UNUSED but keep for now */
  (void)changed;

  return WM_UI_HANDLER_CONTINUE;
}

/* Same as ui_numedit_but_CURVE with some smaller changes. */
static bool ui_numedit_but_CURVEPROFILE(uiBlock *block,
                                        uiBut *but,
                                        uiHandleButtonData *data,
                                        int evtx,
                                        int evty,
                                        bool snap,
                                        const bool shift)
{
  CurveProfile *profile = (CurveProfile *)but->poin;
  CurveProfilePoint *pts = profile->path;
  bool changed = false;

  /* evtx evty and drag coords are absolute mouse-coords,
   * prevents errors when editing when layout changes. */
  int mx = evtx;
  int my = evty;
  ui_window_to_block(data->region, block, &mx, &my);
  int dragx = data->draglastx;
  int dragy = data->draglasty;
  ui_window_to_block(data->region, block, &dragx, &dragy);

  const float zoomx = BLI_rctf_size_x(&but->rect) / BLI_rctf_size_x(&profile->view_rect);
  const float zoomy = BLI_rctf_size_y(&but->rect) / BLI_rctf_size_y(&profile->view_rect);

  if (snap) {
    const float d[2] = {float(mx - data->dragstartx), float(data->dragstarty)};
    if (len_squared_v2(d) < (9.0f * UI_SCALE_FAC)) {
      snap = false;
    }
  }

  float fx = (mx - dragx) / zoomx;
  float fy = (my - dragy) / zoomy;

  if (data->dragsel != -1) {
    float last_x, last_y;
    const float mval_factor = ui_mouse_scale_warp_factor(shift);
    bool moved_point = false; /* for ctrl grid, can't use orig coords because of sorting */

    fx *= mval_factor;
    fy *= mval_factor;

    /* Move all selected points. */
    const float delta[2] = {fx, fy};
    for (int a = 0; a < profile->path_len; a++) {
      /* Don't move the last and first control points. */
      if (pts[a].flag & PROF_SELECT) {
        moved_point |= BKE_curveprofile_move_point(profile, &pts[a], snap, delta);
        last_x = pts[a].x;
        last_y = pts[a].y;
      }
      else {
        /* Move handles when they're selected but the control point isn't. */
        if (ELEM(pts[a].h2, HD_FREE, HD_ALIGN) && pts[a].flag == PROF_H1_SELECT) {
          moved_point |= BKE_curveprofile_move_handle(&pts[a], true, snap, delta);
          last_x = pts[a].h1_loc[0];
          last_y = pts[a].h1_loc[1];
        }
        if (ELEM(pts[a].h2, HD_FREE, HD_ALIGN) && pts[a].flag == PROF_H2_SELECT) {
          moved_point |= BKE_curveprofile_move_handle(&pts[a], false, snap, delta);
          last_x = pts[a].h2_loc[0];
          last_y = pts[a].h2_loc[1];
        }
      }
    }

    BKE_curveprofile_update(profile, PROF_UPDATE_NONE);

    if (moved_point) {
      data->draglastx = evtx;
      data->draglasty = evty;
      changed = true;
#ifdef USE_CONT_MOUSE_CORRECT
      /* NOTE: using 'cmp_last' is weak since there may be multiple points selected,
       * but in practice this isn't really an issue */
      if (ui_but_is_cursor_warp(but)) {
        /* OK but can go outside bounds */
        data->ungrab_mval[0] = but->rect.xmin + ((last_x - profile->view_rect.xmin) * zoomx);
        data->ungrab_mval[1] = but->rect.ymin + ((last_y - profile->view_rect.ymin) * zoomy);
        BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
      }
#endif
    }
    data->dragchange = true; /* mark for selection */
  }
  else {
    /* Clamp the view rect when clipping is on. */
    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.xmin - fx < profile->clip_rect.xmin) {
        fx = profile->view_rect.xmin - profile->clip_rect.xmin;
      }
      else if (profile->view_rect.xmax - fx > profile->clip_rect.xmax) {
        fx = profile->view_rect.xmax - profile->clip_rect.xmax;
      }
      if (profile->view_rect.ymin - fy < profile->clip_rect.ymin) {
        fy = profile->view_rect.ymin - profile->clip_rect.ymin;
      }
      else if (profile->view_rect.ymax - fy > profile->clip_rect.ymax) {
        fy = profile->view_rect.ymax - profile->clip_rect.ymax;
      }
    }

    profile->view_rect.xmin -= fx;
    profile->view_rect.ymin -= fy;
    profile->view_rect.xmax -= fx;
    profile->view_rect.ymax -= fy;

    data->draglastx = evtx;
    data->draglasty = evty;

    changed = true;
  }

  return changed;
}

/**
 * Helper for #ui_do_but_CURVEPROFILE. Used to tell whether to select a control point's handles.
 */
static bool point_draw_handles(CurveProfilePoint *point)
{
  return (point->flag & PROF_SELECT &&
          (ELEM(point->h1, HD_FREE, HD_ALIGN) || ELEM(point->h2, HD_FREE, HD_ALIGN))) ||
         ELEM(point->flag, PROF_H1_SELECT, PROF_H2_SELECT);
}

/**
 * Interaction for curve profile widget.
 * \note Uses hardcoded keys rather than the keymap.
 */
static int ui_do_but_CURVEPROFILE(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  CurveProfile *profile = (CurveProfile *)but->poin;
  int mx = event->xy[0];
  int my = event->xy[1];

  ui_window_to_block(data->region, block, &mx, &my);

  /* Move selected control points. */
  if (event->type == EVT_GKEY && event->val == KM_RELEASE) {
    data->dragstartx = mx;
    data->dragstarty = my;
    data->draglastx = mx;
    data->draglasty = my;
    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    return WM_UI_HANDLER_BREAK;
  }

  /* Delete selected control points. */
  if (event->type == EVT_XKEY && event->val == KM_RELEASE) {
    BKE_curveprofile_remove_by_flag(profile, PROF_SELECT);
    BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
    button_activate_state(C, but, BUTTON_STATE_EXIT);
    return WM_UI_HANDLER_BREAK;
  }

  /* Selecting, adding, and starting point movements. */
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      const float m_xy[2] = {float(mx), float(my)};

      if (event->modifier & KM_CTRL) {
        float f_xy[2];
        BLI_rctf_transform_pt_v(&profile->view_rect, &but->rect, f_xy, m_xy);

        BKE_curveprofile_insert(profile, f_xy[0], f_xy[1]);
        BKE_curveprofile_update(profile, PROF_UPDATE_CLIP);
      }

      /* Check for selecting of a point by finding closest point in radius. */
      CurveProfilePoint *pts = profile->path;
      /* 14 pixels radius for selecting points. */
      float dist_min_sq = square_f(UI_SCALE_FAC * 14.0f);
      int i_selected = -1;
      short selection_type = 0; /* For handle selection. */
      for (int i = 0; i < profile->path_len; i++) {
        float f_xy[2];
        BLI_rctf_transform_pt_v(&but->rect, &profile->view_rect, f_xy, &pts[i].x);
        float dist_sq = len_squared_v2v2(m_xy, f_xy);
        if (dist_sq < dist_min_sq) {
          i_selected = i;
          selection_type = PROF_SELECT;
          dist_min_sq = dist_sq;
        }

        /* Also select handles if the point is selected and it has the right handle type. */
        if (point_draw_handles(&pts[i])) {
          if (ELEM(profile->path[i].h1, HD_FREE, HD_ALIGN)) {
            BLI_rctf_transform_pt_v(&but->rect, &profile->view_rect, f_xy, pts[i].h1_loc);
            dist_sq = len_squared_v2v2(m_xy, f_xy);
            if (dist_sq < dist_min_sq) {
              i_selected = i;
              selection_type = PROF_H1_SELECT;
              dist_min_sq = dist_sq;
            }
          }
          if (ELEM(profile->path[i].h2, HD_FREE, HD_ALIGN)) {
            BLI_rctf_transform_pt_v(&but->rect, &profile->view_rect, f_xy, pts[i].h2_loc);
            dist_sq = len_squared_v2v2(m_xy, f_xy);
            if (dist_sq < dist_min_sq) {
              i_selected = i;
              selection_type = PROF_H2_SELECT;
              dist_min_sq = dist_sq;
            }
          }
        }
      }

      /* Add a point if the click was close to the path but not a control point or handle. */
      if (i_selected == -1) {
        float f_xy[2], f_xy_prev[2];
        CurveProfilePoint *table = profile->table;
        BLI_rctf_transform_pt_v(&but->rect, &profile->view_rect, f_xy, &table[0].x);

        dist_min_sq = square_f(UI_SCALE_FAC * 8.0f); /* 8 pixel radius from each table point. */

        /* Loop through the path's high resolution table and find what's near the click. */
        for (int i = 1; i <= BKE_curveprofile_table_size(profile); i++) {
          copy_v2_v2(f_xy_prev, f_xy);
          BLI_rctf_transform_pt_v(&but->rect, &profile->view_rect, f_xy, &table[i].x);

          if (dist_squared_to_line_segment_v2(m_xy, f_xy_prev, f_xy) < dist_min_sq) {
            BLI_rctf_transform_pt_v(&profile->view_rect, &but->rect, f_xy, m_xy);

            CurveProfilePoint *new_pt = BKE_curveprofile_insert(profile, f_xy[0], f_xy[1]);
            BKE_curveprofile_update(profile, PROF_UPDATE_CLIP);

            /* Get the index of the newly added point. */
            i_selected = int(new_pt - profile->path);
            BLI_assert(i_selected >= 0 && i_selected <= profile->path_len);
            selection_type = PROF_SELECT;
            break;
          }
        }
      }

      /* Change the flag for the point(s) if one was selected or added. */
      if (i_selected != -1) {
        /* Deselect all if this one is deselected, except if we hold shift. */
        if (event->modifier & KM_SHIFT) {
          pts[i_selected].flag ^= selection_type;
        }
        else {
          for (int i = 0; i < profile->path_len; i++) {
            // pts[i].flag &= ~(PROF_SELECT | PROF_H1_SELECT | PROF_H2_SELECT);
            profile->path[i].flag &= ~(PROF_SELECT | PROF_H1_SELECT | PROF_H2_SELECT);
          }
          profile->path[i_selected].flag |= selection_type;
        }
      }
      else {
        /* Move the view. */
        data->cancel = true;
      }

      data->dragsel = i_selected;

      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;

      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) { /* Do control point movement. */
    if (event->type == MOUSEMOVE) {
      if (mx != data->draglastx || my != data->draglasty) {
        if (ui_numedit_but_CURVEPROFILE(
                block, but, data, mx, my, event->modifier & KM_CTRL, event->modifier & KM_SHIFT))
        {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      /* Finish move. */
      if (data->dragsel != -1) {

        if (data->dragchange == false) {
          /* Deselect all, select one. */
        }
        else {
          /* Remove doubles, clip after move. */
          BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
        }
      }
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_HISTOGRAM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
  Histogram *hist = (Histogram *)but->poin;
  const bool changed = true;
  const float dy = my - data->draglasty;

  /* scale histogram values (dy / 10 for better control) */
  const float yfac = min_ff(pow2f(hist->ymax), 1.0f) * 0.5f;
  hist->ymax += (dy * 0.1f) * yfac;

  /* 0.1 allows us to see HDR colors up to 10 */
  CLAMP(hist->ymax, 0.1f, 100.0f);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static int ui_do_but_HISTOGRAM(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_HISTOGRAM(but, data, mx, my)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
    /* XXX hardcoded keymap check.... */
    if (event->type == EVT_BACKSPACEKEY && event->val == KM_PRESS) {
      Histogram *hist = (Histogram *)but->poin;
      hist->ymax = 1.0f;

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == EVT_ESCKEY) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == MOUSEMOVE) {
      if (mx != data->draglastx || my != data->draglasty) {
        if (ui_numedit_but_HISTOGRAM(but, data, mx, my)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_WAVEFORM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
  Scopes *scopes = (Scopes *)but->poin;
  const bool changed = true;

  const float dy = my - data->draglasty;

  /* scale waveform values */
  scopes->wavefrm_yfac += dy / 200.0f;

  CLAMP(scopes->wavefrm_yfac, 0.5f, 2.0f);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static int ui_do_but_WAVEFORM(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_WAVEFORM(but, data, mx, my)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
    /* XXX hardcoded keymap check.... */
    if (event->type == EVT_BACKSPACEKEY && event->val == KM_PRESS) {
      Scopes *scopes = (Scopes *)but->poin;
      scopes->wavefrm_yfac = 1.0f;

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == EVT_ESCKEY) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == MOUSEMOVE) {
      if (mx != data->draglastx || my != data->draglasty) {
        if (ui_numedit_but_WAVEFORM(but, data, mx, my)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_TRACKPREVIEW(
    bContext *C, uiBut *but, uiHandleButtonData *data, int mx, int my, const bool shift)
{
  MovieClipScopes *scopes = (MovieClipScopes *)but->poin;
  const bool changed = true;

  float dx = mx - data->draglastx;
  float dy = my - data->draglasty;

  if (shift) {
    dx /= 5.0f;
    dy /= 5.0f;
  }

  if (!scopes->track_locked) {
    const MovieClip *clip = CTX_data_edit_movieclip(C);
    const int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, scopes->scene_framenr);
    if (scopes->marker->framenr != clip_framenr) {
      scopes->marker = BKE_tracking_marker_ensure(scopes->track, clip_framenr);
    }

    scopes->marker->flag &= ~(MARKER_DISABLED | MARKER_TRACKED);
    scopes->marker->pos[0] += -dx * scopes->slide_scale[0] / BLI_rctf_size_x(&but->block->rect);
    scopes->marker->pos[1] += -dy * scopes->slide_scale[1] / BLI_rctf_size_y(&but->block->rect);

    WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, nullptr);
  }

  scopes->ok = 0;

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static int ui_do_but_TRACKPREVIEW(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->modifier & KM_SHIFT)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == EVT_ESCKEY) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == MOUSEMOVE) {
      if (mx != data->draglastx || my != data->draglasty) {
        if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->modifier & KM_SHIFT)) {
          ui_numedit_apply(C, block, but, data);
        }
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      button_activate_state(C, but, BUTTON_STATE_EXIT);
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_button(bContext *C, uiBlock *block, uiBut *but, const wmEvent *event)
{
  uiHandleButtonData *data = but->active;
  int retval = WM_UI_HANDLER_CONTINUE;

  const bool is_disabled = but->flag & UI_BUT_DISABLED || data->disable_force;

  /* If `but->pointype` is set, `but->poin` should be too. */
  BLI_assert(!bool(but->pointype) || but->poin);

  /* Only hard-coded stuff here, button interactions with configurable
   * keymaps are handled using operators (see #ED_keymap_ui). */

  if (data->state == BUTTON_STATE_HIGHLIGHT) {

    /* handle copy and paste */
    bool is_press_ctrl_but_no_shift = (event->val == KM_PRESS) &&
                                      (event->modifier & (KM_CTRL | KM_OSKEY)) &&
                                      (event->modifier & KM_SHIFT) == 0;
    const bool do_copy = event->type == EVT_CKEY && is_press_ctrl_but_no_shift;
    const bool do_paste = event->type == EVT_VKEY && is_press_ctrl_but_no_shift;

    /* Specific handling for list-rows, we try to find their overlapping text button. */
    if ((do_copy || do_paste) && but->type == ButType::ListRow) {
      uiBut *labelbut = ui_but_list_row_text_activate(C, but, data, event, BUTTON_ACTIVATE_OVER);
      if (labelbut) {
        but = labelbut;
        data = but->active;
      }
    }

    /* do copy first, because it is the only allowed operator when disabled */
    if (do_copy) {
      if (ui_but_copy(C, but, event->modifier & KM_ALT)) {
        return WM_UI_HANDLER_BREAK;
      }
    }

    /* handle menu */

    if ((event->type == RIGHTMOUSE) && (event->modifier == 0) && (event->val == KM_PRESS)) {
      /* For some button types that are typically representing entire sets of data,
       * right-clicking to spawn the context menu should also activate the item. This makes it
       * clear which item will be operated on. Apply the button immediately, so context menu
       * polls get the right active item. */
      uiButViewItem *clicked_view_item_but = static_cast<uiButViewItem *>(
          but->type == ButType::ViewItem ? but :
                                           ui_view_item_find_mouse_over(data->region, event->xy));
      if (clicked_view_item_but) {
        clicked_view_item_but->view_item->activate_for_context_menu(*C);
        ED_region_tag_redraw_no_rebuild(data->region);
      }

      /* RMB has two options now */
      if (ui_popup_context_menu_for_button(C, but, event)) {
        WM_cursor_modal_restore(data->window);
        return WM_UI_HANDLER_BREAK;
      }
    }

    if (is_disabled) {
      return WM_UI_HANDLER_CONTINUE;
    }

#ifdef WITH_INPUT_NDOF
    /* 2D view navigation conflicts with using NDOF to adjust colors,
     * especially in the node-editor, see: #105224. */
    if (event->type == NDOF_MOTION) {
      if (data->region->runtime->type->keymapflag & ED_KEYMAP_VIEW2D) {
        return WM_UI_HANDLER_CONTINUE;
      }
    }
#endif /* WITH_INPUT_NDOF */

    if (do_paste) {
      ui_but_paste(C, but, data, event->modifier & KM_ALT);
      return WM_UI_HANDLER_BREAK;
    }

    if ((data->state == BUTTON_STATE_HIGHLIGHT) &&
        ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, EVT_PADENTER, EVT_RETKEY) &&
        (event->val == KM_RELEASE) &&
        /* Only returns true if the event was handled. */
        ui_do_but_extra_operator_icon(C, but, data, event))
    {
      return WM_UI_HANDLER_BREAK;
    }
  }

  if (but->flag & UI_BUT_DISABLED) {
    /* It's important to continue here instead of breaking since breaking causes the event to be
     * considered "handled", preventing further click/drag events from being generated.
     *
     * An example of where this is needed is dragging node-sockets, where dragging a node-socket
     * could exit the button before the drag threshold was reached, disable the button then break
     * handling of the #MOUSEMOVE event preventing the socket being dragged entirely, see: #96255.
     *
     * Region level event handling is responsible for preventing events being passed
     * through to parts of the UI that are logically behind this button, see: #92364. */
    return WM_UI_HANDLER_CONTINUE;
  }

  switch (but->type) {
    case ButType::But:
    case ButType::Decorator:
      retval = ui_do_but_BUT(C, but, data, event);
      break;
    case ButType::KeyEvent:
      retval = ui_do_but_KEYEVT(C, but, data, event);
      break;
    case ButType::HotkeyEvent:
      retval = ui_do_but_HOTKEYEVT(C, but, data, event);
      break;
    case ButType::Tab:
      retval = ui_do_but_TAB(C, block, but, data, event);
      break;
    case ButType::ButToggle:
    case ButType::Toggle:
    case ButType::IconToggle:
    case ButType::IconToggleN:
    case ButType::ToggleN:
    case ButType::Checkbox:
    case ButType::CheckboxN:
    case ButType::Row:
      retval = ui_do_but_TOG(C, but, data, event);
      break;
    case ButType::ViewItem:
      retval = ui_do_but_VIEW_ITEM(C, but, data, event);
      break;
    case ButType::Scroll:
      retval = ui_do_but_SCROLL(C, block, but, data, event);
      break;
    case ButType::Grip:
      retval = ui_do_but_GRIP(C, block, but, data, event);
      break;
    case ButType::Num:
      retval = ui_do_but_NUM(C, block, but, data, event);
      break;
    case ButType::NumSlider:
      retval = ui_do_but_SLI(C, block, but, data, event);
      break;
    case ButType::ListBox:
      /* Nothing to do! */
      break;
    case ButType::ListRow:
      retval = ui_do_but_LISTROW(C, but, data, event);
      break;
    case ButType::Roundbox:
    case ButType::Label:
    case ButType::Image:
    case ButType::Progress:
    case ButType::NodeSocket:
    case ButType::PreviewTile:
      retval = ui_do_but_EXIT(C, but, data, event);
      break;
    case ButType::Histogram:
      retval = ui_do_but_HISTOGRAM(C, block, but, data, event);
      break;
    case ButType::Waveform:
      retval = ui_do_but_WAVEFORM(C, block, but, data, event);
      break;
    case ButType::Vectorscope:
      /* Nothing to do! */
      break;
    case ButType::Text:
    case ButType::SearchMenu:
      if ((but->type == ButType::SearchMenu) && (but->flag & UI_BUT_VALUE_CLEAR)) {
        retval = ui_do_but_SEARCH_UNLINK(C, block, but, data, event);
        if (retval & WM_UI_HANDLER_BREAK) {
          break;
        }
      }
      retval = ui_do_but_TEX(C, block, but, data, event);
      break;
    case ButType::Menu:
    case ButType::Popover:
    case ButType::Block:
    case ButType::Pulldown:
      retval = ui_do_but_BLOCK(C, but, data, event);
      break;
    case ButType::ButMenu:
      retval = ui_do_but_BUT(C, but, data, event);
      break;
    case ButType::Color:
      retval = ui_do_but_COLOR(C, but, data, event);
      break;
    case ButType::Unitvec:
      retval = ui_do_but_UNITVEC(C, block, but, data, event);
      break;
    case ButType::ColorBand:
      retval = ui_do_but_COLORBAND(C, block, but, data, event);
      break;
    case ButType::Curve:
      retval = ui_do_but_CURVE(C, block, but, data, event);
      break;
    case ButType::CurveProfile:
      retval = ui_do_but_CURVEPROFILE(C, block, but, data, event);
      break;
    case ButType::HsvCube:
      retval = ui_do_but_HSVCUBE(C, block, but, data, event);
      break;
    case ButType::HsvCircle:
      retval = ui_do_but_HSVCIRCLE(C, block, but, data, event);
      break;
    case ButType::TrackPreview:
      retval = ui_do_but_TRACKPREVIEW(C, block, but, data, event);
      break;

      /* quiet warnings for unhandled types */
    case ButType::Sepr:
    case ButType::SeprLine:
    case ButType::SeprSpacer:
    case ButType::Extra:
      break;
  }

#ifdef USE_DRAG_MULTINUM
  data = but->active;
  if (data) {
    if (ISMOUSE_MOTION(event->type) ||
        /* if we started dragging, progress on any event */
        (data->multi_data.init == uiHandleButtonMulti::INIT_SETUP))
    {
      if (ELEM(but->type, ButType::Num, ButType::NumSlider) &&
          ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING))
      {
        /* initialize! */
        if (data->multi_data.init == uiHandleButtonMulti::INIT_UNSET) {
          /* --> (uiHandleButtonMulti::INIT_SETUP | uiHandleButtonMulti::INIT_DISABLE) */

          const float margin_y = DRAG_MULTINUM_THRESHOLD_DRAG_Y / sqrtf(block->aspect);

          /* check if we have a vertical gesture */
          if (len_squared_v2(data->multi_data.drag_dir) > (margin_y * margin_y)) {
            const float dir_nor_y[2] = {0.0, 1.0f};
            float dir_nor_drag[2];

            normalize_v2_v2(dir_nor_drag, data->multi_data.drag_dir);

            if (fabsf(dot_v2v2(dir_nor_drag, dir_nor_y)) > DRAG_MULTINUM_THRESHOLD_VERTICAL) {
              data->multi_data.init = uiHandleButtonMulti::INIT_SETUP;
              data->multi_data.drag_lock_x = event->xy[0];
            }
            else {
              data->multi_data.init = uiHandleButtonMulti::INIT_DISABLE;
            }
          }
        }
        else if (data->multi_data.init == uiHandleButtonMulti::INIT_SETUP) {
          /* --> (uiHandleButtonMulti::INIT_ENABLE) */
          const float margin_x = DRAG_MULTINUM_THRESHOLD_DRAG_X / sqrtf(block->aspect);
          /* Check if we're don't setting buttons. */
          if ((data->text_edit.edit_string &&
               ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING)) ||
              ((abs(data->multi_data.drag_lock_x - event->xy[0]) > margin_x) &&
               /* Just to be sure, check we're dragging more horizontally then vertically. */
               abs(event->prev_xy[0] - event->xy[0]) > abs(event->prev_xy[1] - event->xy[1])))
          {
            if (data->multi_data.has_mbuts) {
              ui_multibut_states_create(but, data);
              data->multi_data.init = uiHandleButtonMulti::INIT_ENABLE;
            }
            else {
              data->multi_data.init = uiHandleButtonMulti::INIT_DISABLE;
            }
          }
        }

        if (data->multi_data.init == uiHandleButtonMulti::INIT_SETUP) {
          if (ui_multibut_states_tag(but, data, event)) {
            ED_region_tag_redraw(data->region);
          }
        }
      }
    }
  }
#endif /* USE_DRAG_MULTINUM */

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Tool Tip
 * \{ */

static void ui_blocks_set_tooltips(ARegion *region, const bool enable)
{
  if (!region) {
    return;
  }

  /* We disabled buttons when they were already shown, and re-enable them on mouse move. */
  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    block->tooltipdisabled = !enable;
  }
}

void UI_but_tooltip_refresh(bContext *C, uiBut *but)
{
  uiHandleButtonData *data = but->active;
  if (data) {
    bScreen *screen = WM_window_get_active_screen(data->window);
    if (screen->tool_tip && screen->tool_tip->region) {
      WM_tooltip_refresh(C, data->window);
    }
  }
}

void UI_but_tooltip_timer_remove(bContext *C, uiBut *but)
{
  uiHandleButtonData *data = but->active;
  if (data) {
    if (data->autoopentimer) {
      WM_event_timer_remove(data->wm, data->window, data->autoopentimer);
      data->autoopentimer = nullptr;
    }

    if (data->window) {
      WM_tooltip_clear(C, data->window);
    }
  }
}

static ARegion *ui_but_tooltip_init(
    bContext *C, ARegion *region, int *pass, double *r_pass_delay, bool *r_exit_on_event)
{
  bool is_quick_tip = false;
  if (*pass == 1) {
    is_quick_tip = true;
    (*pass)--;
    (*r_pass_delay) = UI_TOOLTIP_DELAY - UI_TOOLTIP_DELAY_QUICK;
  }

  uiBut *but = UI_region_active_but_get(region);
  *r_exit_on_event = false;
  if (but) {
    const wmWindow *win = CTX_wm_window(C);
    uiButExtraOpIcon *extra_icon = ui_but_extra_operator_icon_mouse_over_get(
        but, but->active ? but->active->region : region, win->eventstate);

    return UI_tooltip_create_from_button_or_extra_icon(C, region, but, extra_icon, is_quick_tip);
  }
  return nullptr;
}

static void button_tooltip_timer_reset(bContext *C, uiBut *but)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  uiHandleButtonData *data = but->active;

  WM_tooltip_timer_clear(C, data->window);

  if ((U.flag & USER_TOOLTIPS) || (data->tooltip_force)) {
    if (!but->block->tooltipdisabled) {
      if (!wm->runtime->drags.first) {
        const bool is_quick_tip = UI_but_has_quick_tooltip(but);
        const double delay = is_quick_tip ? UI_TOOLTIP_DELAY_QUICK : UI_TOOLTIP_DELAY;
        WM_tooltip_timer_init_ex(
            C, data->window, data->area, data->region, ui_but_tooltip_init, delay);
        if (is_quick_tip) {
          bScreen *screen = WM_window_get_active_screen(data->window);
          if (screen->tool_tip) {
            screen->tool_tip->pass = screen->tool_tip->region ? 0 : 1;
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button State Handling
 * \{ */

static bool button_modal_state(uiHandleButtonState state)
{
  return ELEM(state,
              BUTTON_STATE_WAIT_RELEASE,
              BUTTON_STATE_WAIT_KEY_EVENT,
              BUTTON_STATE_NUM_EDITING,
              BUTTON_STATE_TEXT_EDITING,
              BUTTON_STATE_TEXT_SELECTING,
              BUTTON_STATE_MENU_OPEN);
}

static void button_activate_state(bContext *C, uiBut *but, uiHandleButtonState state)
{
  uiHandleButtonData *data = but->active;
  if (data->state == state) {
    return;
  }

  /* Highlight has timers for tool-tips and auto open. */
  if (state == BUTTON_STATE_HIGHLIGHT) {
    but->flag &= ~UI_SELECT;

    button_tooltip_timer_reset(C, but);

    /* Automatic open pull-down block timer. */
    if (ELEM(but->type, ButType::Block, ButType::Pulldown, ButType::Popover) ||
        /* Menu button types may draw as popovers, check for this case
         * ignoring other kinds of menus (mainly enums). (see #66538). */
        ((but->type == ButType::Menu) &&
         (UI_but_paneltype_get(but) || ui_but_menu_draw_as_popover(but))))
    {
      if (data->used_mouse && !data->autoopentimer) {
        int time;

        if (but->block->auto_open == true) { /* test for toolbox */
          time = 1;
        }
        else if ((but->block->flag & UI_BLOCK_LOOP && but->type != ButType::Block) ||
                 (but->block->auto_open == true))
        {
          time = 5 * U.menuthreshold2;
        }
        else if (U.uiflag & USER_MENUOPENAUTO) {
          time = 5 * U.menuthreshold1;
        }
        else {
          time = -1; /* do nothing */
        }

        if (time >= 0) {
          data->autoopentimer = WM_event_timer_add(
              data->wm, data->window, TIMER, 0.02 * double(time));
        }
      }
    }
  }
  else {
    but->flag |= UI_SELECT;
    UI_but_tooltip_timer_remove(C, but);
  }

  /* text editing */
  if (state == BUTTON_STATE_TEXT_EDITING && data->state != BUTTON_STATE_TEXT_SELECTING) {
    ui_textedit_begin(C, but, data);
  }
  else if (data->state == BUTTON_STATE_TEXT_EDITING && state != BUTTON_STATE_TEXT_SELECTING) {
    ui_textedit_end(C, but, data);
  }
  else if (data->state == BUTTON_STATE_TEXT_SELECTING && state != BUTTON_STATE_TEXT_EDITING) {
    ui_textedit_end(C, but, data);
  }

  /* number editing */
  if (state == BUTTON_STATE_NUM_EDITING) {
    if (ui_but_is_cursor_warp(but)) {
      if (ELEM(but->type, ButType::HsvCircle, ButType::HsvCube)) {
        rctf rectf;
        ui_block_to_window_rctf(data->region, but->block, &rectf, &but->rect);
        rcti bounds;
        BLI_rcti_rctf_copy(&bounds, &rectf);
        WM_cursor_grab_enable(CTX_wm_window(C), WM_CURSOR_WRAP_XY, &bounds, true);
      }
      else {
        WM_cursor_grab_enable(CTX_wm_window(C), WM_CURSOR_WRAP_XY, nullptr, true);
      }
      WorkspaceStatus status(C);
      status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
#if defined(__APPLE__)
      status.item(IFACE_("Snap"), ICON_KEY_COMMAND);
#else
      status.item(IFACE_("Snap"), ICON_EVENT_CTRL);
#endif
      if (ui_but_is_float(but)) {
        status.item(IFACE_("Precision"), ICON_EVENT_SHIFT);
      }
    }
    ui_numedit_begin(but, data);
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    ui_numedit_end(but, data);

    if (state != BUTTON_STATE_TEXT_EDITING) {
      ED_workspace_status_text(C, nullptr);
    }

    if (but->flag & UI_BUT_DRIVEN) {
      /* Only warn when editing stepping/dragging the value.
       * No warnings should show for editing driver expressions though!
       */
      if (state != BUTTON_STATE_TEXT_EDITING) {
        WM_global_report(RPT_INFO,
                         "Can't edit driven number value, see driver editor for the driver setup");
      }
    }

    if (ui_but_is_cursor_warp(but)) {

#ifdef USE_CONT_MOUSE_CORRECT
      /* stereo3d has issues with changing cursor location so rather avoid */
      if (data->ungrab_mval[0] != FLT_MAX && !WM_stereo3d_enabled(data->window, false)) {
        int mouse_ungrab_xy[2];
        ui_block_to_window_fl(
            data->region, but->block, &data->ungrab_mval[0], &data->ungrab_mval[1]);
        mouse_ungrab_xy[0] = data->ungrab_mval[0];
        mouse_ungrab_xy[1] = data->ungrab_mval[1];

        WM_cursor_grab_disable(data->window, mouse_ungrab_xy);
      }
      else {
        WM_cursor_grab_disable(data->window, nullptr);
      }
#else
      WM_cursor_grab_disable(data->window, nullptr);
#endif
    }
  }
  /* menu open */
  if (state == BUTTON_STATE_MENU_OPEN) {
    ui_block_open_begin(C, but, data);
  }
  else if (data->state == BUTTON_STATE_MENU_OPEN) {
    ui_block_open_end(C, but, data);
  }

  /* add a short delay before exiting, to ensure there is some feedback */
  if (state == BUTTON_STATE_WAIT_FLASH) {
    data->flashtimer = WM_event_timer_add(data->wm, data->window, TIMER, BUTTON_FLASH_DELAY);
  }
  else if (data->flashtimer) {
    WM_event_timer_remove(data->wm, data->window, data->flashtimer);
    data->flashtimer = nullptr;
  }

  /* add hold timer if it's used */
  if (state == BUTTON_STATE_WAIT_RELEASE && (but->hold_func != nullptr)) {
    data->hold_action_timer = WM_event_timer_add(
        data->wm, data->window, TIMER, BUTTON_AUTO_OPEN_THRESH);
  }
  else if (data->hold_action_timer) {
    WM_event_timer_remove(data->wm, data->window, data->hold_action_timer);
    data->hold_action_timer = nullptr;
  }

  /* Add a blocking ui handler at the window handler for blocking, modal states
   * but not for popups, because we already have a window level handler. */
  if (!(but->block->handle && but->block->handle->popup)) {
    if (button_modal_state(state)) {
      if (!button_modal_state(data->state)) {
        WM_event_add_ui_handler(C,
                                &data->window->modalhandlers,
                                ui_handler_region_menu,
                                nullptr,
                                data,
                                eWM_EventHandlerFlag(0));
      }
    }
    else {
      if (button_modal_state(data->state)) {
        /* true = postpone free */
        WM_event_remove_ui_handler(
            &data->window->modalhandlers, ui_handler_region_menu, nullptr, data, true);
      }
    }
  }

  /* Wait for mouse-move to enable drag. */
  if (state == BUTTON_STATE_WAIT_DRAG) {
    but->flag &= ~UI_SELECT;
  }

  if (state == BUTTON_STATE_TEXT_EDITING) {
    ui_block_interaction_begin_ensure(C, but->block, data, true);
  }
  else if (state == BUTTON_STATE_EXIT) {
    if (data->state == BUTTON_STATE_NUM_EDITING) {
      /* This happens on pasting values for example. */
      ui_block_interaction_begin_ensure(C, but->block, data, true);
    }
  }

  data->state = state;

  if (state != BUTTON_STATE_EXIT) {
    /* When objects for eg. are removed, running ui_but_update() can access
     * the removed data - so disable update on exit. Also in case of
     * highlight when not in a popup menu, we remove because data used in
     * button below popup might have been removed by action of popup. Needs
     * a more reliable solution... */
    if (state != BUTTON_STATE_HIGHLIGHT || (but->block->flag & UI_BLOCK_LOOP)) {
      ui_but_update(but);
    }
  }

  /* redraw */
  ED_region_tag_redraw_no_rebuild(data->region);
}

static void button_activate_init(bContext *C,
                                 ARegion *region,
                                 uiBut *but,
                                 uiButtonActivateType type)
{
  /* Don't activate semi-modal buttons the normal way, they have special activation handling. */
  if (but->semi_modal_state) {
    return;
  }
  /* Only ever one active button! */
  BLI_assert(ui_region_find_active_but(region) == nullptr);

  /* setup struct */
  uiHandleButtonData *data = MEM_new<uiHandleButtonData>(__func__);
  data->wm = CTX_wm_manager(C);
  data->window = CTX_wm_window(C);
  data->area = CTX_wm_area(C);
  BLI_assert(region != nullptr);
  data->region = region;

#ifdef USE_CONT_MOUSE_CORRECT
  copy_v2_fl(data->ungrab_mval, FLT_MAX);
#endif

  if (ELEM(but->type, ButType::Curve, ButType::CurveProfile, ButType::SearchMenu)) {
    /* XXX curve is temp */
  }
  else {
    if ((but->flag & UI_BUT_UPDATE_DELAY) == 0) {
      data->interactive = true;
    }
  }

  data->state = BUTTON_STATE_INIT;

  /* Activate button. Sets the hover flag to enable button highlights, usually the button is
   * initially activated because it's hovered. */
  but->flag |= UI_HOVER;
  but->active = data;

  /* we disable auto_open in the block after a threshold, because we still
   * want to allow auto opening adjacent menus even if no button is activated
   * in between going over to the other button, but only for a short while */
  if (type == BUTTON_ACTIVATE_OVER && but->block->auto_open == true) {
    if (but->block->auto_open_last + BUTTON_AUTO_OPEN_THRESH < BLI_time_now_seconds()) {
      but->block->auto_open = false;
    }
  }

  if (type == BUTTON_ACTIVATE_OVER) {
    data->used_mouse = true;
  }
  button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);

  if (type == BUTTON_ACTIVATE_OPEN) {
    button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);

    /* activate first button in submenu */
    if (data->menu && data->menu->region) {
      ARegion *subar = data->menu->region;
      uiBlock *subblock = static_cast<uiBlock *>(subar->runtime->uiblocks.first);
      uiBut *subbut;

      if (subblock) {
        subbut = ui_but_first(subblock);

        if (subbut) {
          ui_handle_button_activate(C, subar, subbut, BUTTON_ACTIVATE);
        }
      }
    }
  }
  else if (type == BUTTON_ACTIVATE_TEXT_EDITING) {
    button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
  }
  else if (type == BUTTON_ACTIVATE_APPLY) {
    button_activate_state(C, but, BUTTON_STATE_WAIT_FLASH);
  }

  if (but->type == ButType::Grip) {
    const bool horizontal = (BLI_rctf_size_x(&but->rect) < BLI_rctf_size_y(&but->rect));
    WM_cursor_modal_set(data->window, horizontal ? WM_CURSOR_X_MOVE : WM_CURSOR_Y_MOVE);
  }
  else if (but->type == ButType::Num) {
    ui_numedit_set_active(but);
  }

  if (UI_but_has_quick_tooltip(but)) {
    /* Show a label for this button. */
    bScreen *screen = WM_window_get_active_screen(data->window);
    if ((BLI_time_now_seconds() - WM_tooltip_time_closed()) < 0.1) {
      WM_tooltip_immediate_init(C, CTX_wm_window(C), data->area, region, ui_but_tooltip_init);
      if (screen->tool_tip) {
        screen->tool_tip->pass = 1;
      }
    }
  }
}

static void button_activate_exit(
    bContext *C, uiBut *but, uiHandleButtonData *data, const bool mousemove, const bool onfree)
{
  wmWindow *win = data->window;
  uiBlock *block = but->block;

  if (but->type == ButType::Grip) {
    WM_cursor_modal_restore(win);
  }

  /* ensure we are in the exit state */
  if (data->state != BUTTON_STATE_EXIT) {
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }

  /* apply the button action or value */
  if (!onfree) {
    ui_apply_but(C, block, but, data, false);
  }

#ifdef USE_DRAG_MULTINUM
  if (data->multi_data.has_mbuts) {
    for (const std::unique_ptr<uiBut> &bt : block->buttons) {
      if (bt->flag & UI_BUT_DRAG_MULTI) {
        bt->flag &= ~UI_BUT_DRAG_MULTI;

        if (!data->cancel) {
          ui_apply_but_autokey(C, bt.get());
        }
      }
    }

    ui_multibut_free(data, block);
  }
#endif

  /* if this button is in a menu, this will set the button return
   * value to the button value and the menu return value to ok, the
   * menu return value will be picked up and the menu will close */
  if (block->handle && !(block->flag & UI_BLOCK_KEEP_OPEN)) {
    if (!data->cancel || data->escapecancel) {
      uiPopupBlockHandle *menu;

      menu = block->handle;
      menu->butretval = data->retval;
      menu->menuretval = (data->cancel) ? UI_RETURN_CANCEL : UI_RETURN_OK;
    }
  }

  if (!onfree && !data->cancel) {
    /* autokey & undo push */
    ui_apply_but_undo(but);
    ui_apply_but_autokey(C, but);

#ifdef USE_ALLSELECT
    {
      /* only RNA from this button is used */
      uiBut but_temp = *but;
      uiSelectContextStore *selctx_data = &data->select_others;
      for (uiSelectContextElem &other : selctx_data->elems) {
        but_temp.rnapoin = other.ptr;
        ui_apply_but_autokey(C, &but_temp);
      }
    }
#endif

    /* popup menu memory */
    if (block->flag & UI_BLOCK_POPUP_MEMORY) {
      ui_popup_menu_memory_set(block, but);
    }

    if (U.runtime.is_dirty == false) {
      ui_but_update_preferences_dirty(but);
    }
  }

  /* Disable tool-tips until mouse-move + last active flag. */
  LISTBASE_FOREACH (uiBlock *, block_iter, &data->region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &bt : block_iter->buttons) {
      bt->flag &= ~UI_BUT_LAST_ACTIVE;
    }

    block_iter->tooltipdisabled = true;
  }

  ui_blocks_set_tooltips(data->region, false);

  /* clean up */
  if (data->text_edit.edit_string) {
    MEM_freeN(data->text_edit.edit_string);
  }
  if (data->text_edit.original_string) {
    MEM_freeN(data->text_edit.original_string);
  }

#ifdef USE_ALLSELECT
  ui_selectcontext_end(but, &data->select_others);
#endif

  if (data->changed_cursor) {
    WM_cursor_modal_restore(win);
  }

  /* redraw and refresh (for popups) */
  ED_region_tag_redraw_no_rebuild(data->region);
  ED_region_tag_refresh_ui(data->region);

  if ((but->flag & UI_BUT_DRAG_MULTI) == 0) {
    if (data->custom_interaction_handle != nullptr) {
      /* Should only set when the button is modal. */
      BLI_assert(but->active != nullptr);
      data->custom_interaction_handle->user_count--;

      BLI_assert(data->custom_interaction_handle->user_count >= 0);
      if (data->custom_interaction_handle->user_count == 0) {
        ui_block_interaction_end(
            C, &but->block->custom_interaction_callbacks, data->custom_interaction_handle);
      }
      data->custom_interaction_handle = nullptr;
    }
  }

  BLI_assert(!but->semi_modal_state || but->semi_modal_state == but->active);
  but->semi_modal_state = nullptr;
  /* clean up button */
  ui_but_handle_data_free(&but->active);

  but->flag &= ~(UI_HOVER | UI_SELECT);
  but->flag |= UI_BUT_LAST_ACTIVE;
  if (!onfree) {
    ui_but_update(but);
  }

  /* Adds empty mouse-move in queue for re-initialize handler, in case mouse is
   * still over a button. We cannot just check for this ourselves because
   * at this point the mouse may be over a button in another region. */
  if (mousemove) {
    WM_event_add_mousemove(CTX_wm_window(C));
  }
}

void ui_but_active_free(const bContext *C, uiBut *but)
{
  /* this gets called when the button somehow disappears while it is still
   * active, this is bad for user interaction, but we need to handle this
   * case cleanly anyway in case it happens */
  if (but->active) {
    uiHandleButtonData *data = but->active;
    data->cancel = true;
    button_activate_exit((bContext *)C, but, data, false, true);
  }
}

void ui_but_semi_modal_state_free(const bContext *C, uiBut *but)
{
  if (!but->semi_modal_state) {
    return;
  }
  /* Activate the button (using the semi modal state) and use the normal active button freeing. */
  with_but_active_as_semi_modal(const_cast<bContext *>(C),
                                but->semi_modal_state->region,
                                but,
                                [&]() { ui_but_active_free(C, but); });
}

/* returns the active button with an optional checking function */
static uiBut *ui_context_button_active(const ARegion *region, bool (*but_check_cb)(const uiBut *))
{
  uiBut *but_found = nullptr;

  while (region) {
    /* Follow this exact priority (from highest to lowest priority):
     * 1) Active-override button (#UI_BUT_ACTIVE_OVERRIDE).
     * 2) The real active button.
     * 3) The previously active button (#UI_BUT_LAST_ACTIVE).
     */
    uiBut *active_but_override = nullptr;
    uiBut *active_but_real = nullptr;
    uiBut *active_but_last = nullptr;

    /* find active button */
    LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
      for (const std::unique_ptr<uiBut> &but : block->buttons) {
        if (but->flag & UI_BUT_ACTIVE_OVERRIDE) {
          active_but_override = but.get();
        }
        if (but->active) {
          active_but_real = but.get();
        }
        if (but->flag & UI_BUT_LAST_ACTIVE) {
          active_but_last = but.get();
        }
      }
    }

    uiBut *activebut = active_but_override;
    if (!activebut) {
      activebut = active_but_real;
    }
    if (!activebut) {
      activebut = active_but_last;
    }

    if (activebut && (but_check_cb == nullptr || but_check_cb(activebut))) {
      uiHandleButtonData *data = activebut->active;

      but_found = activebut;

      /* Recurse into opened menu, like color-picker case. */
      if (data && data->menu && (region != data->menu->region)) {
        region = data->menu->region;
      }
      else {
        return but_found;
      }
    }
    else {
      /* no active button */
      return but_found;
    }
  }

  return but_found;
}

uiBut *UI_context_active_but_get(const bContext *C)
{
  return ui_context_button_active(CTX_wm_region(C), nullptr);
}

uiBut *UI_context_active_but_get_respect_popup(const bContext *C)
{
  ARegion *region_popup = CTX_wm_region_popup(C);
  return ui_context_button_active(region_popup ? region_popup : CTX_wm_region(C), nullptr);
}

uiBut *UI_region_active_but_get(const ARegion *region)
{
  return ui_context_button_active(region, nullptr);
}

uiBut *UI_region_but_find_rect_over(const ARegion *region, const rcti *rect_px)
{
  return ui_but_find_rect_over(region, rect_px);
}

uiBlock *UI_region_block_find_mouse_over(const ARegion *region, const int xy[2], bool only_clip)
{
  return ui_block_find_mouse_over_ex(region, xy, only_clip);
}

uiBut *UI_region_active_but_prop_get(const ARegion *region,
                                     PointerRNA *r_ptr,
                                     PropertyRNA **r_prop,
                                     int *r_index)
{
  uiBut *activebut = UI_region_active_but_get(region);

  if (activebut && activebut->rnapoin.data) {
    *r_ptr = activebut->rnapoin;
    *r_prop = activebut->rnaprop;
    *r_index = activebut->rnaindex;
  }
  else {
    *r_ptr = {};
    *r_prop = nullptr;
    *r_index = 0;
  }

  return activebut;
}

uiBut *UI_context_active_but_prop_get(const bContext *C,
                                      PointerRNA *r_ptr,
                                      PropertyRNA **r_prop,
                                      int *r_index)
{
  ARegion *region_popup = CTX_wm_region_popup(C);
  return UI_region_active_but_prop_get(
      region_popup ? region_popup : CTX_wm_region(C), r_ptr, r_prop, r_index);
}

void UI_context_active_but_prop_handle(bContext *C, const bool handle_undo)
{
  uiBut *activebut = UI_context_active_but_get_respect_popup(C);
  if (activebut) {
    /* TODO(@ideasman42): look into a better way to handle the button change
     * currently this is mainly so reset defaults works for the
     * operator redo panel. */
    uiBlock *block = activebut->block;

    /* There are various functions buttons may run on completion.
     * See #ui_apply_but_funcs_after for reference, we could even call
     * this function however it may have unforeseen consequences,
     * see replies to: !134233. */

    /* This may be needed to validate the value, see: #134101. */
    if (activebut->func) {
      activebut->func(C, activebut->func_arg1, activebut->func_arg2);
    }
    if (block->handle_func) {
      block->handle_func(C, block->handle_func_arg, activebut->retval);
    }
    if (handle_undo) {
      /* Update the button so the undo text uses the correct value. */
      ui_but_update(activebut);
      ui_apply_but_undo(activebut);
    }
  }
}

void UI_context_active_but_clear(bContext *C, wmWindow *win, ARegion *region)
{
  wm_event_handler_ui_cancel_ex(C, win, region, false);
}

wmOperator *UI_context_active_operator_get(const bContext *C)
{
  ARegion *region_ctx = CTX_wm_region(C);

  /* background mode */
  if (region_ctx == nullptr) {
    return nullptr;
  }

  /* scan active regions ui */
  LISTBASE_FOREACH (uiBlock *, block, &region_ctx->runtime->uiblocks) {
    if (block->ui_operator) {
      return block->ui_operator;
    }
  }

  /* scan popups */
  {
    bScreen *screen = CTX_wm_screen(C);

    LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
      if (region == region_ctx) {
        continue;
      }
      LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
        if (block->ui_operator) {
          return block->ui_operator;
        }
      }
    }
  }

  return nullptr;
}

ARegion *UI_region_searchbox_region_get(const ARegion *button_region)
{
  uiBut *but = UI_region_active_but_get(button_region);
  return (but != nullptr) ? but->active->searchbox : nullptr;
}

void UI_context_update_anim_flag(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, (scene) ? BKE_scene_frame_get(scene) : 0.0f);

  while (region) {
    /* find active button */
    uiBut *activebut = nullptr;

    LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
      for (const std::unique_ptr<uiBut> &but : block->buttons) {
        ui_but_anim_flag(but.get(), &anim_eval_context);
        ui_but_override_flag(CTX_data_main(C), but.get());
        if (UI_but_is_decorator(but)) {
          ui_but_anim_decorate_update_from_flag((uiButDecorator *)but.get());
        }

        ED_region_tag_redraw(region);

        if (but->active) {
          activebut = but.get();
        }
        else if (!activebut && (but->flag & UI_BUT_LAST_ACTIVE)) {
          activebut = but.get();
        }
      }
    }

    if (activebut) {
      /* Always recurse into opened menu, so all buttons update (like color-picker). */
      uiHandleButtonData *data = activebut->active;
      if (data && data->menu) {
        region = data->menu->region;
      }
      else {
        return;
      }
    }
    else {
      /* no active button */
      return;
    }
  }
}

void ui_but_update_view_for_active(const bContext *C, const uiBlock *block)
{
  uiBut *active_but = ui_block_active_but_get(block);
  if (!active_but || !active_but->active || !active_but->changed || active_but->block != block) {
    return;
  }
  /* If there is a search popup attached to the button, don't change the view. The popups don't
   * support updating the position to the button position nicely. */
  uiHandleButtonData *data = active_but->active;
  if (data->searchbox) {
    return;
  }

  UI_but_ensure_in_view(C, active_but->active->region, active_but);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Activation Handling
 * \{ */

static uiBut *ui_but_find_open_event(ARegion *region, const wmEvent *event)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but.get() == event->customdata) {
        return but.get();
      }
    }
  }
  return nullptr;
}

static int ui_handle_button_over(bContext *C, const wmEvent *event, ARegion *region)
{
  if (event->type == MOUSEMOVE) {
    const bool labeledit = event->modifier & KM_CTRL;
    /* Allow buttons to be activated to show the tool-tip,
     * then force-disable them if they're not considered interactive
     * so they don't swallow events but can still display tips. */
    const bool for_tooltip = true;
    uiBut *but = ui_but_find_mouse_over_ex(
        region, event->xy, labeledit, for_tooltip, nullptr, nullptr);
    if (but) {
      button_activate_init(C, region, but, BUTTON_ACTIVATE_OVER);

      if ((event->modifier & KM_ALT) && but->active) {
        /* Display tool-tips if holding Alt on mouse-over when tool-tips are disabled in the
         * preferences. */
        but->active->tooltip_force = true;
      }

      if (but->active && !ui_but_is_interactive(but, labeledit)) {
        but->active->disable_force = true;
      }
    }
  }
  else if (event->type == EVT_BUT_OPEN) {
    uiBut *but = ui_but_find_open_event(region, event);
    if (but) {
      button_activate_init(C, region, but, BUTTON_ACTIVATE_OVER);
      ui_do_button(C, but->block, but, event);
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

void ui_but_activate_event(bContext *C, ARegion *region, uiBut *but)
{
  wmWindow *win = CTX_wm_window(C);

  button_activate_init(C, region, but, BUTTON_ACTIVATE_OVER);

  wmEvent event;
  wm_event_init_from_window(win, &event);
  event.type = EVT_BUT_OPEN;
  event.val = KM_PRESS;
  event.flag = eWM_EventFlag(0);
  event.customdata = but;
  event.customdata_free = false;

  ui_do_button(C, but->block, but, &event);
}

void ui_but_activate_over(bContext *C, ARegion *region, uiBut *but)
{
  button_activate_init(C, region, but, BUTTON_ACTIVATE_OVER);
}

void ui_but_execute_begin(bContext * /*C*/, ARegion *region, uiBut *but, void **active_back)
{
  BLI_assert(region != nullptr);
  BLI_assert(BLI_findindex(&region->runtime->uiblocks, but->block) != -1);
  /* NOTE: ideally we would not have to change 'but->active' however
   * some functions we call don't use data (as they should be doing) */
  uiHandleButtonData *data;
  *active_back = but->active;
  data = MEM_new<uiHandleButtonData>(__func__);
  but->active = data;
  BLI_assert(region != nullptr);
  data->region = region;
}

void ui_but_execute_end(bContext *C, ARegion * /*region*/, uiBut *but, void *active_back)
{
  ui_apply_but(C, but->block, but, but->active, true);

  if ((but->flag & UI_BUT_DRAG_MULTI) == 0) {
    ui_apply_but_autokey(C, but);
  }
  /* use onfree event so undo is handled by caller and apply is already done above */
  button_activate_exit(C, but, but->active, false, true);
  but->active = static_cast<uiHandleButtonData *>(active_back);
}

static void ui_handle_button_activate(bContext *C,
                                      ARegion *region,
                                      uiBut *but,
                                      uiButtonActivateType type)
{
  uiBut *oldbut = ui_region_find_active_but(region);
  if (oldbut) {
    uiHandleButtonData *data = oldbut->active;
    data->cancel = true;
    button_activate_exit(C, oldbut, data, false, false);
  }

  button_activate_init(C, region, but, type);
}

/**
 * Use for key accelerator or default key to activate the button even if its not active.
 */
static bool ui_handle_button_activate_by_type(bContext *C, ARegion *region, uiBut *but)
{
  if (ELEM(but->type, ButType::ButMenu, ButType::Row)) {
    /* mainly for operator buttons */
    ui_handle_button_activate(C, region, but, BUTTON_ACTIVATE_APPLY);
  }
  else if (ELEM(but->type, ButType::Block, ButType::Pulldown)) {
    /* open sub-menus (like right arrow key) */
    ui_handle_button_activate(C, region, but, BUTTON_ACTIVATE_OPEN);
  }
  else if (but->type == ButType::Menu) {
    /* activate menu items */
    ui_handle_button_activate(C, region, but, BUTTON_ACTIVATE);
  }
  else {
#ifndef NDEBUG
    CLOG_WARN(&LOG, "%s: error, unhandled type: %d", __func__, int(but->type));
#endif
    return false;
  }
  return true;
}

/**
 * Calls \a fn with \a but activated for semi-modal handling.
 *
 * Button handling code requires the button to be active, but at the same time only one active
 * button per region is supported. So if there's a different active button already, it needs to be
 * deactivated temporarily (by unsetting its #uiBut.active member and restoring it when done).
 *
 * During the \fn call, the passed \a but will appear to be the active button of the region, i.e.
 * #ui_region_find_active_but() will return \a but.
 */
static void with_but_active_as_semi_modal(bContext *C,
                                          ARegion *region,
                                          uiBut *but,
                                          blender::FunctionRef<void()> fn)
{
  BLI_assert(but->active == nullptr);

  uiBut *prev_active_but = ui_region_find_active_but(region);
  uiHandleButtonData *prev_active_data = prev_active_but ? prev_active_but->active : nullptr;
  if (prev_active_but) {
    prev_active_but->active = nullptr;
  }

  /* Enforce the button to actually be active, using #uiBut.semi_modal_state to store its handling
   * state. */
  if (!but->semi_modal_state) {
    ui_but_activate_event(C, region, but);
    but->semi_modal_state = but->active;
    but->semi_modal_state->is_semi_modal = true;
  }

  /* Activate the button using the previously created/stored semi-modal state. */
  but->active = but->semi_modal_state;
  fn();
  but->active = nullptr;

  if (prev_active_but) {
    prev_active_but->active = prev_active_data;
  }
}

/**
 * Calls \a fn for all buttons that are either already semi-modal active or should be made to be
 * because the #UI_BUT2_FORCE_SEMI_MODAL_ACTIVE flag is set. During the \a fn call, the button will
 * appear to be the active button, i.e. #ui_region_find_active_but() will return this button.
 */
static void foreach_semi_modal_but_as_active(bContext *C,
                                             ARegion *region,
                                             blender::FunctionRef<void(uiBut *semi_modal_but)> fn)
{
  /* Might want to have some way to define which order these should be handled in - if there's
   * every actually a use-case for multiple semi-active buttons at the same time. */

  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if ((but->flag2 & UI_BUT2_FORCE_SEMI_MODAL_ACTIVE) || but->semi_modal_state) {
        with_but_active_as_semi_modal(C, region, but.get(), [&]() { fn(but.get()); });
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handle Events for Activated Buttons
 * \{ */

static bool ui_button_value_default(uiBut *but, double *r_value)
{
  if (but->rnaprop != nullptr && ui_but_is_rna_valid(but)) {
    const int type = RNA_property_type(but->rnaprop);
    if (ELEM(type, PROP_FLOAT, PROP_INT)) {
      double default_value;
      switch (type) {
        case PROP_INT:
          if (RNA_property_array_check(but->rnaprop)) {
            default_value = double(
                RNA_property_int_get_default_index(&but->rnapoin, but->rnaprop, but->rnaindex));
          }
          else {
            default_value = double(RNA_property_int_get_default(&but->rnapoin, but->rnaprop));
          }
          break;
        case PROP_FLOAT:
          if (RNA_property_array_check(but->rnaprop)) {
            default_value = double(
                RNA_property_float_get_default_index(&but->rnapoin, but->rnaprop, but->rnaindex));
          }
          else {
            default_value = double(RNA_property_float_get_default(&but->rnapoin, but->rnaprop));
          }
          break;
      }
      *r_value = default_value;
      return true;
    }
  }
  return false;
}

static int ui_handle_button_event(bContext *C, const wmEvent *event, uiBut *but)
{
  uiHandleButtonData *data = but->active;
  const uiHandleButtonState state_orig = data->state;

  uiBlock *block = but->block;
  ARegion *region = data->region;

  int retval = WM_UI_HANDLER_CONTINUE;

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    switch (event->type) {
      case WINDEACTIVATE:
      case EVT_BUT_CANCEL:
        data->cancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        break;
#ifdef USE_UI_POPOVER_ONCE
      case LEFTMOUSE: {
        if (event->val == KM_RELEASE) {
          if (block->flag & UI_BLOCK_POPOVER_ONCE) {
            if (!(but->flag & UI_BUT_DISABLED)) {
              if (ui_but_is_popover_once_compat(but)) {
                data->cancel = false;
                button_activate_state(C, but, BUTTON_STATE_EXIT);
                retval = WM_UI_HANDLER_BREAK;
                /* Cancel because this `but` handles all events and we don't want
                 * the parent button's update function to do anything.
                 *
                 * Causes issues with buttons defined by #uiLayout::prop_with_popover. */
                block->handle->menuretval = UI_RETURN_CANCEL;
              }
              else if (ui_but_is_editable_as_text(but)) {
                ui_handle_button_activate(C, region, but, BUTTON_ACTIVATE_TEXT_EDITING);
                retval = WM_UI_HANDLER_BREAK;
              }
            }
          }
        }
        break;
      }
#endif
      case MOUSEMOVE: {
        uiBut *but_other = UI_but_find_mouse_over(region, event);
        bool exit = false;

        /* always deactivate button for pie menus,
         * else moving to blank space will leave activated */
        if ((!ui_block_is_menu(block) || ui_block_is_pie_menu(block)) &&
            !ui_but_contains_point_px(but, region, event->xy))
        {
          exit = true;
        }
        else if (but_other && ui_but_is_editable(but_other) && (but_other != but)) {
          exit = true;
        }

        if (exit) {
          data->cancel = true;
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        else {
          /* Re-enable tool-tip on mouse move. */
          bool reenable_tooltip = true;
          bScreen *screen = CTX_wm_screen(C);
          if (screen && screen->tool_tip) {
            /* Allow some movement once the tooltip timer has started. */
            const int threshold = WM_event_drag_threshold(event);
            const int movement = len_manhattan_v2v2_int(event->xy, screen->tool_tip->event_xy);
            reenable_tooltip = (movement > threshold);
          }
          if (reenable_tooltip) {
            ui_blocks_set_tooltips(region, true);
            button_tooltip_timer_reset(C, but);
          }
        }

        /* Update extra icons states. */
        ui_do_but_extra_operator_icons_mousemove(but, data, event);

        break;
      }
      case TIMER: {
        /* Handle menu auto open timer. */
        if (event->customdata == data->autoopentimer) {
          WM_event_timer_remove(data->wm, data->window, data->autoopentimer);
          data->autoopentimer = nullptr;

          if (ui_but_contains_point_px(but, region, event->xy) || but->active) {
            button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
          }
        }

        break;
      }
      /* XXX hardcoded keymap check... but anyway,
       * while view changes, tool-tips should be removed */
      case WHEELUPMOUSE:
      case WHEELDOWNMOUSE:
      case MIDDLEMOUSE:
      case MOUSEPAN:
        UI_but_tooltip_timer_remove(C, but);
        ATTR_FALLTHROUGH;
      default:
        break;
    }

    /* handle button type specific events */
    retval = ui_do_button(C, block, but, event);
  }
  else if (data->state == BUTTON_STATE_WAIT_RELEASE) {
    switch (event->type) {
      case WINDEACTIVATE:
        data->cancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        break;

      case TIMER: {
        if (event->customdata == data->hold_action_timer) {
          if (true) {
            data->cancel = true;
            button_activate_state(C, but, BUTTON_STATE_EXIT);
          }
          else {
            /* Do this so we can still mouse-up, closing the menu and running the button.
             * This is nice to support but there are times when the button gets left pressed.
             * Keep disabled for now. */
            WM_event_timer_remove(data->wm, data->window, data->hold_action_timer);
            data->hold_action_timer = nullptr;
          }
          retval = WM_UI_HANDLER_CONTINUE;
          but->hold_func(C, data->region, but);
        }
        break;
      }
      case MOUSEMOVE: {
        /* deselect the button when moving the mouse away */
        /* also de-activate for buttons that only show highlights */
        if (ui_but_contains_point_px(but, region, event->xy)) {

          /* Drag on a hold button (used in the toolbar) now opens it immediately. */
          if (data->hold_action_timer) {
            if (but->flag & UI_SELECT) {
              const int threshold = WM_event_drag_threshold(event);
              const int movement = len_manhattan_v2v2_int(event->xy, event->prev_press_xy);
              if (movement <= threshold) {
                /* pass */
              }
              else {
                WM_event_timer_remove(data->wm, data->window, data->hold_action_timer);
                data->hold_action_timer = WM_event_timer_add(data->wm, data->window, TIMER, 0.0f);
              }
            }
          }

          if (!(but->flag & UI_SELECT)) {
            but->flag |= (UI_SELECT | UI_HOVER);
            data->cancel = false;
            ED_region_tag_redraw_no_rebuild(data->region);
          }
        }
        else {
          if (but->flag & UI_SELECT) {
            but->flag &= ~(UI_SELECT | UI_HOVER);
            data->cancel = true;
            ED_region_tag_redraw_no_rebuild(data->region);
          }
        }
        break;
      }
      default:
        /* otherwise catch mouse release event */
        ui_do_button(C, block, but, event);
        break;
    }

    retval = WM_UI_HANDLER_BREAK;
  }
  else if (data->state == BUTTON_STATE_WAIT_FLASH) {
    switch (event->type) {
      case TIMER: {
        if (event->customdata == data->flashtimer) {
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        break;
      }
      default: {
        break;
      }
    }

    retval = WM_UI_HANDLER_CONTINUE;
  }
  else if (data->state == BUTTON_STATE_MENU_OPEN) {
    /* check for exit because of mouse-over another button */
    switch (event->type) {
      case MOUSEMOVE: {
        uiBut *bt;

        if (data->menu && data->menu->region) {
          if (ui_region_contains_point_px(data->menu->region, event->xy)) {
            break;
          }
        }

        bt = UI_but_find_mouse_over(region, event);

        if (bt && bt->active != data) {
          if (but->type != ButType::Color) { /* exception */
            data->cancel = true;
          }
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        break;
      }
      case RIGHTMOUSE: {
        if (event->val == KM_PRESS) {
          uiBut *bt = UI_but_find_mouse_over(region, event);
          if (bt && bt->active == data) {
            button_activate_state(C, bt, BUTTON_STATE_HIGHLIGHT);
          }
        }
        break;
      }
      default: {
        break;
      }
    }

    ui_do_button(C, block, but, event);
    retval = WM_UI_HANDLER_CONTINUE;
  }
  else {
    retval = ui_do_button(C, block, but, event);
    // retval = WM_UI_HANDLER_BREAK; XXX why ?
  }

  /* may have been re-allocated above (eyedropper for eg) */
  data = but->active;
  if (data && data->state == BUTTON_STATE_EXIT) {
    uiBut *post_but = data->postbut;
    const uiButtonActivateType post_type = data->posttype;

    /* Reset the button value when empty text is typed. */
    if ((data->cancel == false) && (data->text_edit.edit_string != nullptr) &&
        (data->text_edit.edit_string[0] == '\0') &&
        (but->rnaprop && ELEM(RNA_property_type(but->rnaprop), PROP_FLOAT, PROP_INT)))
    {
      MEM_SAFE_FREE(data->text_edit.edit_string);
      ui_button_value_default(but, &data->value);

#ifdef USE_DRAG_MULTINUM
      if (data->multi_data.mbuts) {
        for (LinkNode *l = data->multi_data.mbuts; l; l = l->next) {
          uiButMultiState *state = static_cast<uiButMultiState *>(l->link);
          uiBut *but_iter = state->but;
          double default_value;

          if (ui_button_value_default(but_iter, &default_value)) {
            ui_but_value_set(but_iter, default_value);
          }
        }
      }
      data->multi_data.skip = true;
#endif
    }

    button_activate_exit(C, but, data, (post_but == nullptr), false);

    /* for jumping to the next button with tab while text editing */
    if (post_but) {
      /* The post_but still has previous ranges (without the changes in active button considered),
       * needs refreshing the ranges. */
      ui_but_range_set_soft(post_but);
      ui_but_range_set_hard(post_but);

      button_activate_init(C, region, post_but, post_type);
    }
    else if (!((event->type == EVT_BUT_CANCEL) && (event->val == 1))) {
      /* XXX issue is because WM_event_add_mousemove(wm) is a bad hack and not reliable,
       * if that gets coded better this bypass can go away too.
       *
       * This is needed to make sure if a button was active,
       * it stays active while the mouse is over it.
       * This avoids adding mouse-moves, see: #33466. */
      if (ELEM(state_orig, BUTTON_STATE_INIT, BUTTON_STATE_HIGHLIGHT, BUTTON_STATE_WAIT_DRAG)) {
        if (UI_but_find_mouse_over(region, event) == but) {
          button_activate_init(C, region, but, BUTTON_ACTIVATE_OVER);
        }
      }
    }
  }

  return retval;
}

static int ui_list_get_increment(const uiList *ui_list, const int type, const int columns)
{
  int increment = 0;

  /* Handle column offsets for grid layouts. */
  if (ELEM(type, EVT_UPARROWKEY, EVT_DOWNARROWKEY) &&
      ELEM(ui_list->layout_type, UILST_LAYOUT_BIG_PREVIEW_GRID))
  {
    increment = (type == EVT_UPARROWKEY) ? -columns : columns;
  }
  else {
    /* Left or right in grid layouts or any direction in single column layouts increments by 1. */
    increment = ELEM(type, EVT_UPARROWKEY, EVT_LEFTARROWKEY, WHEELUPMOUSE) ? -1 : 1;
  }

  if ((ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) != 0) {
    increment *= -1;
  }

  return increment;
}

static int ui_handle_list_event(bContext *C, const wmEvent *event, ARegion *region, uiBut *listbox)
{
  int retval = WM_UI_HANDLER_CONTINUE;
  int type = event->type, val = event->val;
  int scroll_dir = 1;
  bool redraw = false;

  uiList *ui_list = static_cast<uiList *>(listbox->custom_data);
  if (!ui_list || !ui_list->dyn_data) {
    return retval;
  }
  uiListDyn *dyn_data = ui_list->dyn_data;

  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(region, listbox->block, &mx, &my);

  /* Convert pan to scroll-wheel. */
  if (type == MOUSEPAN) {
    ui_pan_to_scroll(event, &type, &val);

    /* 'ui_pan_to_scroll' gives the absolute direction. */
    if (event->flag & WM_EVENT_SCROLL_INVERT) {
      scroll_dir = -1;
    }

    /* If type still is mouse-pan, we call it handled, since delta-y accumulate. */
    /* also see `wm_event_system.cc` do_wheel_ui hack. */
    if (type == MOUSEPAN) {
      retval = WM_UI_HANDLER_BREAK;
    }
  }

  if (val == KM_PRESS) {
    if ((ELEM(type, EVT_UPARROWKEY, EVT_DOWNARROWKEY, EVT_LEFTARROWKEY, EVT_RIGHTARROWKEY) &&
         (event->modifier == 0)) ||
        (ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE) && (event->modifier == KM_CTRL)))
    {
      const int value_orig = RNA_property_int_get(&listbox->rnapoin, listbox->rnaprop);
      int value, min, max;

      value = value_orig;
      const int inc = ui_list_get_increment(ui_list, type, dyn_data->columns);

      if (dyn_data->items_filter_neworder || dyn_data->items_filter_flags) {
        /* If we have a display order different from
         * collection order, we have some work! */
        int *org_order = MEM_malloc_arrayN<int>(dyn_data->items_shown, __func__);
        const int *new_order = dyn_data->items_filter_neworder;
        int org_idx = -1, len = dyn_data->items_len;
        int current_idx = -1;

        for (int i = 0; i < len; i++) {
          if (UI_list_item_index_is_filtered_visible(ui_list, i)) {
            org_order[new_order ? new_order[++org_idx] : ++org_idx] = i;
            if (i == value) {
              current_idx = new_order ? new_order[org_idx] : org_idx;
            }
          }
          else if (i == value && org_idx >= 0) {
            current_idx = -(new_order ? new_order[org_idx] : org_idx) - 1;
          }
        }
        /* Now, org_order maps displayed indices to real indices,
         * and current_idx either contains the displayed index of active value (positive),
         *                 or its more-nearest one (negated).
         */
        if (current_idx < 0) {
          current_idx = (current_idx * -1) + (inc < 0 ? inc : inc - 1);
        }
        else {
          current_idx += inc;
        }
        CLAMP(current_idx, 0, dyn_data->items_shown - 1);
        value = org_order[current_idx];
        MEM_freeN(org_order);
      }
      else {
        value += inc;
      }

      CLAMP(value, 0, dyn_data->items_len - 1);

      RNA_property_int_range(&listbox->rnapoin, listbox->rnaprop, &min, &max);
      CLAMP(value, min, max);

      if (value != value_orig) {
        RNA_property_int_set(&listbox->rnapoin, listbox->rnaprop, value);
        RNA_property_update(C, &listbox->rnapoin, listbox->rnaprop);

        ui_apply_but_undo(listbox);

        ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
        redraw = true;
      }
      retval = WM_UI_HANDLER_BREAK;
    }
    else if (ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE) && (event->modifier & KM_SHIFT)) {
      /* We now have proper grip, but keep this anyway! */
      if (ui_list->list_grip < (dyn_data->visual_height_min - UI_LIST_AUTO_SIZE_THRESHOLD)) {
        ui_list->list_grip = dyn_data->visual_height;
      }
      ui_list->list_grip += (type == WHEELUPMOUSE) ? -1 : 1;

      ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;

      redraw = true;
      retval = WM_UI_HANDLER_BREAK;
    }
    else if (ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE)) {
      if (dyn_data->height > dyn_data->visual_height) {
        /* list template will clamp */
        ui_list->list_scroll += scroll_dir * ((type == WHEELUPMOUSE) ? -1 : 1);

        redraw = true;
        retval = WM_UI_HANDLER_BREAK;
      }
    }
  }

  if (redraw) {
    ED_region_tag_redraw(region);
    ED_region_tag_refresh_ui(region);
  }

  return retval;
}

/* Handle mouse hover for Views and UiList rows. */
static int ui_handle_viewlist_items_hover(const wmEvent *event, ARegion *region)
{
  const bool has_list = !BLI_listbase_is_empty(&region->ui_lists);
  const bool has_view = [&]() {
    LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
      if (!BLI_listbase_is_empty(&block->views)) {
        return true;
      }
    }
    return false;
  }();

  if (!has_view && !has_list) {
    /* Avoid unnecessary lookup. */
    return WM_UI_HANDLER_CONTINUE;
  }

  /* Always highlight the hovered view item, even if the mouse hovers another button inside. */
  uiBut *highlight_row_but = [&]() -> uiBut * {
    if (uiBut *but = ui_view_item_find_search_highlight(region)) {
      return but;
    }
    if (uiBut *but = ui_view_item_find_mouse_over(region, event->xy)) {
      return but;
    }
    if (uiBut *but = ui_list_row_find_mouse_over(region, event->xy)) {
      return but;
    }
    return nullptr;
  }();

  bool changed = false;

  if (highlight_row_but && !(highlight_row_but->flag & UI_HOVER)) {
    highlight_row_but->flag |= UI_HOVER;
    changed = true;
  }

  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but.get() == highlight_row_but) {
        continue;
      }
      if (!ELEM(but->type, ButType::ViewItem, ButType::ListRow)) {
        continue;
      }

      if (but->flag & UI_HOVER) {
        but->flag &= ~UI_HOVER;
        changed = true;
      }
    }
  }

  if (changed) {
    ED_region_tag_redraw_no_rebuild(region);
  }

  return WM_UI_HANDLER_CONTINUE;
}

static int ui_handle_view_item_event(bContext *C,
                                     const wmEvent *event,
                                     uiBut *active_but,
                                     ARegion *region)
{
  switch (event->type) {
    case MOUSEMOVE:
      if (event->xy[0] != event->prev_xy[0] || event->xy[1] != event->prev_xy[1]) {
        UI_region_views_clear_search_highlight(region);
      }
      break;
    case LEFTMOUSE:
      if (event->modifier == 0) {
        /* Only bother finding the active view item button if the active button isn't already a
         * view item. */
        uiButViewItem *view_but = static_cast<uiButViewItem *>(
            (active_but && active_but->type == ButType::ViewItem) ?
                active_but :
                ui_view_item_find_mouse_over(region, event->xy));

        if (view_but) {
          if (UI_view_item_supports_drag(*view_but->view_item)) {
            if (event->val != KM_CLICK) {
              break;
            }
          }
          else if (event->val != KM_PRESS) {
            break;
          }

          /* Will free active button if there already is one. */
          /* Close the popup when clicking on the view item directly, not any overlapped button. */
          const bool close_popup = view_but == active_but;
          force_activate_view_item_but(C, region, view_but, close_popup);
        }
      }
      break;
    case EVT_RETKEY:
    case EVT_PADENTER:
      if (event->val == KM_PRESS) {
        if (uiButViewItem *search_highlight_but = static_cast<uiButViewItem *>(
                ui_view_item_find_search_highlight(region)))
        {
          force_activate_view_item_but(C, region, search_highlight_but);
          return WM_UI_HANDLER_BREAK;
        }
      }
      break;
    default:
      break;
  }

  return WM_UI_HANDLER_CONTINUE;
}

static void ui_handle_button_return_submenu(bContext *C, const wmEvent *event, uiBut *but)
{
  uiHandleButtonData *data = but->active;
  uiPopupBlockHandle *menu = data->menu;

  /* copy over return values from the closing menu */
  if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_UPDATE)) {
    if (but->type == ButType::Color) {
      copy_v3_v3(data->vec, menu->retvec);
    }
    else if (but->type == ButType::Menu) {
      data->value = menu->retvalue;
    }
  }

  if (menu->menuretval & UI_RETURN_UPDATE) {
    if (data->interactive) {
      ui_apply_but(C, but->block, but, data, true);
    }
    else {
      ui_but_update(but);
    }

    menu->menuretval = 0;
  }

  /* now change button state or exit, which will close the submenu */
  if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_CANCEL)) {
    if (menu->menuretval != UI_RETURN_OK) {
      data->cancel = true;
    }

    button_activate_exit(C, but, data, true, false);
  }
  else if (menu->menuretval & UI_RETURN_OUT) {
    if (event->type == MOUSEMOVE && ui_but_contains_point_px(but, data->region, event->xy)) {
      button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
    }
    else {
      if (ISKEYBOARD(event->type)) {
        /* keyboard menu hierarchy navigation, going back to previous level */
        but->active->used_mouse = false;
        button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
      }
      else {
        data->cancel = true;
        button_activate_exit(C, but, data, true, false);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Towards (mouse motion logic)
 * \{ */

/**
 * Function used to prevent losing the open menu when using nested pull-downs,
 * when moving mouse towards the pull-down menu over other buttons that could
 * steal the highlight from the current button, only checks:
 *
 * - while mouse moves in triangular area defined old mouse position and
 *   left/right side of new menu.
 * - only for 1 second.
 */

static void ui_mouse_motion_towards_init_ex(uiPopupBlockHandle *menu,
                                            const int xy[2],
                                            const bool force)
{
  BLI_assert(((uiBlock *)menu->region->runtime->uiblocks.first)->flag &
             (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER));

  if (!menu->dotowards || force) {
    menu->dotowards = true;
    menu->towards_xy[0] = xy[0];
    menu->towards_xy[1] = xy[1];

    if (force) {
      menu->towardstime = DBL_MAX; /* unlimited time */
    }
    else {
      menu->towardstime = BLI_time_now_seconds();
    }
  }
}

static void ui_mouse_motion_towards_init(uiPopupBlockHandle *menu, const int xy[2])
{
  ui_mouse_motion_towards_init_ex(menu, xy, false);
}

static void ui_mouse_motion_towards_reinit(uiPopupBlockHandle *menu, const int xy[2])
{
  ui_mouse_motion_towards_init_ex(menu, xy, true);
}

static bool ui_mouse_motion_towards_check(uiBlock *block,
                                          uiPopupBlockHandle *menu,
                                          const int xy[2],
                                          const bool use_wiggle_room)
{
  BLI_assert(block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER));

  /* annoying fix for #36269, this is a bit odd but in fact works quite well
   * don't mouse-out of a menu if another menu has been created after it.
   * if this causes problems we could remove it and check on a different fix - campbell */
  if (menu->region->next) {
    /* Test if this is the last menu. */
    ARegion *region = menu->region->next;
    do {
      uiBlock *block_iter = static_cast<uiBlock *>(region->runtime->uiblocks.first);
      if (block_iter && ui_block_is_menu(block_iter)) {
        return true;
      }
    } while ((region = region->next));
  }
  /* annoying fix end! */

  if (!menu->dotowards) {
    return false;
  }

  float oldp[2] = {menu->towards_xy[0], menu->towards_xy[1]};
  const float newp[2] = {float(xy[0]), float(xy[1])};
  if (len_squared_v2v2(oldp, newp) < (4.0f * 4.0f)) {
    return menu->dotowards;
  }

  /* verify that we are moving towards one of the edges of the
   * menu block, in other words, in the triangle formed by the
   * initial mouse location and two edge points. */
  rctf rect_px;
  ui_block_to_window_rctf(menu->region, block, &rect_px, &block->rect);

  const float margin = MENU_TOWARDS_MARGIN;

  const float p1[2] = {rect_px.xmin - margin, rect_px.ymin - margin};
  const float p2[2] = {rect_px.xmax + margin, rect_px.ymin - margin};
  const float p3[2] = {rect_px.xmax + margin, rect_px.ymax + margin};
  const float p4[2] = {rect_px.xmin - margin, rect_px.ymax + margin};

  /* allow for some wiggle room, if the user moves a few pixels away,
   * don't immediately quit (only for top level menus) */
  if (use_wiggle_room) {
    const float cent[2] = {BLI_rctf_cent_x(&rect_px), BLI_rctf_cent_y(&rect_px)};
    float delta[2];

    sub_v2_v2v2(delta, oldp, cent);
    normalize_v2_length(delta, MENU_TOWARDS_WIGGLE_ROOM);
    add_v2_v2(oldp, delta);
  }

  bool closer = (isect_point_tri_v2(newp, oldp, p1, p2) ||
                 isect_point_tri_v2(newp, oldp, p2, p3) ||
                 isect_point_tri_v2(newp, oldp, p3, p4) || isect_point_tri_v2(newp, oldp, p4, p1));

  if (!closer) {
    menu->dotowards = false;
  }

  /* 1 second timer */
  if (BLI_time_now_seconds() - menu->towardstime > BUTTON_MOUSE_TOWARDS_THRESH) {
    menu->dotowards = false;
  }

  return menu->dotowards;
}

#ifdef USE_KEYNAV_LIMIT
static void ui_mouse_motion_keynav_init(uiKeyNavLock *keynav, const wmEvent *event)
{
  keynav->is_keynav = true;
  copy_v2_v2_int(keynav->event_xy, event->xy);
}
/**
 * Return true if key-input isn't blocking mouse-motion,
 * or if the mouse-motion is enough to disable key-input.
 */
static bool ui_mouse_motion_keynav_test(uiKeyNavLock *keynav, const wmEvent *event)
{
  if (keynav->is_keynav &&
      (len_manhattan_v2v2_int(keynav->event_xy, event->xy) > BUTTON_KEYNAV_PX_LIMIT))
  {
    keynav->is_keynav = false;
  }

  return keynav->is_keynav;
}
#endif /* USE_KEYNAV_LIMIT */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Scroll
 * \{ */

static char ui_menu_scroll_test(uiBlock *block, int my)
{
  if (block->flag & (UI_BLOCK_CLIPTOP | UI_BLOCK_CLIPBOTTOM)) {
    if (block->flag & UI_BLOCK_CLIPTOP) {
      if (my > block->rect.ymax - UI_MENU_SCROLL_MOUSE) {
        return 't';
      }
    }
    if (block->flag & UI_BLOCK_CLIPBOTTOM) {
      if (my < block->rect.ymin + UI_MENU_SCROLL_MOUSE) {
        return 'b';
      }
    }
  }
  return 0;
}

static void ui_menu_scroll_apply_offset_y(ARegion *region, uiBlock *block, float dy)
{
  BLI_assert(dy != 0.0f);

  const int scroll_pad = ui_block_is_menu(block) ? UI_MENU_SCROLL_PAD : UI_UNIT_Y * 0.5f;

  if (dy < 0.0f) {
    /* Stop at top item, extra 0.5 UI_UNIT_Y makes it snap nicer. */
    float ymax = -FLT_MAX;
    for (const std::unique_ptr<uiBut> &bt : block->buttons) {
      ymax = max_ff(ymax, bt->rect.ymax);
    }
    if (ymax + dy - UI_UNIT_Y * 0.5f < block->rect.ymax - scroll_pad) {
      dy = block->rect.ymax - ymax - scroll_pad;
    }
  }
  else {
    /* Stop at bottom item, extra 0.5 UI_UNIT_Y makes it snap nicer. */
    float ymin = FLT_MAX;
    for (const std::unique_ptr<uiBut> &bt : block->buttons) {
      ymin = min_ff(ymin, bt->rect.ymin);
    }
    if (ymin + dy + UI_UNIT_Y * 0.5f > block->rect.ymin + scroll_pad) {
      dy = block->rect.ymin - ymin + scroll_pad;
    }
  }

  /* remember scroll offset for refreshes */
  block->handle->scrolloffset += dy;
  /* Apply popup scroll delta to layout panels too. */
  ui_layout_panel_popup_scroll_apply(block->panel, dy);

  /* apply scroll offset */
  for (const std::unique_ptr<uiBut> &bt : block->buttons) {
    bt->rect.ymin += dy;
    bt->rect.ymax += dy;
  }

  /* set flags again */
  ui_popup_block_scrolltest(block);

  ED_region_tag_redraw(region);
}

/** Scroll to activated button. */
static bool ui_menu_scroll_to_but(ARegion *region, uiBlock *block, uiBut *but_target)
{
  float dy = 0.0;
  if (block->flag & UI_BLOCK_CLIPTOP) {
    if (but_target->rect.ymax > block->rect.ymax - UI_MENU_SCROLL_ARROW) {
      dy = block->rect.ymax - but_target->rect.ymax - UI_MENU_SCROLL_ARROW;
    }
  }
  if (block->flag & UI_BLOCK_CLIPBOTTOM) {
    if (but_target->rect.ymin < block->rect.ymin + UI_MENU_SCROLL_ARROW) {
      dy = block->rect.ymin - but_target->rect.ymin + UI_MENU_SCROLL_ARROW;
    }
  }
  if (dy != 0.0f) {
    ui_menu_scroll_apply_offset_y(region, block, dy);
    return true;
  }
  return false;
}

/** Scroll to y location (in block space, see #ui_window_to_block). */
static bool ui_menu_scroll_to_y(ARegion *region, uiBlock *block, int y)
{
  const char test = ui_menu_scroll_test(block, y);
  float dy = 0.0f;
  if (test == 't') {
    dy = -UI_UNIT_Y; /* scroll to the top */
  }
  else if (test == 'b') {
    dy = UI_UNIT_Y; /* scroll to the bottom */
  }
  if (dy != 0.0f) {
    ui_menu_scroll_apply_offset_y(region, block, dy);
    return true;
  }
  return false;
}

static bool ui_menu_scroll_step(ARegion *region, uiBlock *block, const int scroll_dir)
{
  int my;
  if (scroll_dir == 1) {
    if ((block->flag & UI_BLOCK_CLIPTOP) == 0) {
      return false;
    }
    my = block->rect.ymax + UI_UNIT_Y;
  }
  else if (scroll_dir == -1) {
    if ((block->flag & UI_BLOCK_CLIPBOTTOM) == 0) {
      return false;
    }
    my = block->rect.ymin - UI_UNIT_Y;
  }
  else {
    BLI_assert(0);
    return false;
  }

  return ui_menu_scroll_to_y(region, block, my);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Event Handling
 * \{ */

static void ui_region_auto_open_clear(ARegion *region)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    block->auto_open = false;
  }
}

/**
 * Special function to handle nested menus.
 * let the parent menu get the event.
 *
 * This allows a menu to be open,
 * but send key events to the parent if there's no active buttons.
 *
 * Without this keyboard navigation from menus won't work.
 */
static bool ui_menu_pass_event_to_parent_if_nonactive(uiPopupBlockHandle *menu,
                                                      const uiBut *but,
                                                      const int level,
                                                      const bool is_parent_menu,
                                                      const int retval)
{
  /* NOTE(@ideasman42): For `menu->popup` (not a nested tree of menus), don't pass events parents.
   * This is needed because enum popups (for example) aren't created with an active button.
   * Otherwise opening a popup & pressing the accelerator key would fail, see: #107838. */
  if ((level != 0) && (but == nullptr) && (is_parent_menu || menu->popup == false)) {
    menu->menuretval = UI_RETURN_OUT | UI_RETURN_OUT_PARENT;
    (void)retval; /* so release builds with strict flags are happy as well */
    BLI_assert(retval == WM_UI_HANDLER_CONTINUE);
    return true;
  }
  return false;
}

static int ui_handle_menu_button(bContext *C, const wmEvent *event, uiPopupBlockHandle *menu)
{
  ARegion *region = menu->region;
  uiBut *but = ui_region_find_active_but(region);

  if (but) {
    /* Its possible there is an active menu item NOT under the mouse,
     * in this case ignore mouse clicks outside the button (but Enter etc is accepted) */
    if (event->val == KM_RELEASE) {
      /* pass, needed so we can exit active menu-items when click-dragging out of them */
    }
    else if (but->type == ButType::SearchMenu) {
      /* Pass, needed so search popup can have RMB context menu.
       * This may be useful for other interactions which happen in the search popup
       * without being directly over the search button. */
    }
    else if (!ui_block_is_menu(but->block) || ui_block_is_pie_menu(but->block)) {
      /* pass, skip for dialogs */
    }
    else if (!ui_region_contains_point_px(but->active->region, event->xy)) {
      /* Pass, needed to click-exit outside of non-floating menus. */
      ui_region_auto_open_clear(but->active->region);
    }
    else if (ISMOUSE_BUTTON(event->type)) {
      if (!ui_but_contains_point_px(but, but->active->region, event->xy)) {
        but = nullptr;
      }
    }
  }

  int retval;
  if (but) {
    ScrArea *ctx_area = CTX_wm_area(C);
    ARegion *ctx_region = CTX_wm_region(C);

    if (menu->ctx_area) {
      CTX_wm_area_set(C, menu->ctx_area);
    }
    if (menu->ctx_region) {
      CTX_wm_region_set(C, menu->ctx_region);
    }

    retval = ui_handle_button_event(C, event, but);

    if (menu->ctx_area) {
      CTX_wm_area_set(C, ctx_area);
    }
    if (menu->ctx_region) {
      CTX_wm_region_set(C, ctx_region);
    }
  }
  else {
    retval = ui_handle_button_over(C, event, region);
  }

  return retval;
}

float ui_block_calc_pie_segment(uiBlock *block, const float event_xy[2])
{
  float seg1[2];

  if (block->pie_data.flags & UI_PIE_INITIAL_DIRECTION) {
    copy_v2_v2(seg1, block->pie_data.pie_center_init);
  }
  else {
    copy_v2_v2(seg1, block->pie_data.pie_center_spawned);
  }

  float seg2[2];
  sub_v2_v2v2(seg2, event_xy, seg1);

  const float len = normalize_v2_v2(block->pie_data.pie_dir, seg2);

  if (len < U.pie_menu_threshold * UI_SCALE_FAC) {
    block->pie_data.flags |= UI_PIE_INVALID_DIR;
  }
  else {
    block->pie_data.flags &= ~UI_PIE_INVALID_DIR;
  }

  return len;
}

static int ui_handle_menu_letter_press_search(uiPopupBlockHandle *menu, const wmEvent *event)
{
  /* Start menu search if the menu has a name. */
  if (menu->menu_idname[0]) {
    uiAfterFunc *after = ui_afterfunc_new();
    wmOperatorType *ot = WM_operatortype_find("WM_OT_search_single_menu", false);
    after->optype = ot;
    after->opcontext = blender::wm::OpCallContext::InvokeDefault;
    after->opptr = MEM_new<PointerRNA>(__func__);
    WM_operator_properties_create_ptr(after->opptr, ot);
    RNA_string_set(after->opptr, "menu_idname", menu->menu_idname);
    if (event->type != EVT_SPACEKEY) {
      /* Forward all keys except space-bar to the search. */
      const int num_bytes = BLI_str_utf8_size_or_error(event->utf8_buf);
      if (num_bytes != -1) {
        char buf[sizeof(event->utf8_buf) + 1];
        memcpy(buf, event->utf8_buf, num_bytes);
        buf[num_bytes] = '\0';
        RNA_string_set(after->opptr, "initial_query", buf);
      }
    }
    menu->menuretval = UI_RETURN_OK;
    return WM_UI_HANDLER_BREAK;
  }
  return WM_UI_HANDLER_CONTINUE;
}

static int ui_handle_menu_event(bContext *C,
                                const wmEvent *event,
                                uiPopupBlockHandle *menu,
                                int level,
                                const bool is_parent_inside,
                                const bool is_parent_menu,
                                const bool is_floating)
{
  uiBut *but;
  ARegion *region = menu->region;
  uiBlock *block = static_cast<uiBlock *>(region->runtime->uiblocks.first);

  int retval = WM_UI_HANDLER_CONTINUE;

  int mx = event->xy[0];
  int my = event->xy[1];
  ui_window_to_block(region, block, &mx, &my);

  /* check if mouse is inside block */
  const bool inside = BLI_rctf_isect_pt(&block->rect, mx, my);
  /* check for title dragging */
  const bool inside_title = inside && ((my + (UI_UNIT_Y * 1.4f)) > block->rect.ymax);

  /* if there's an active modal button, don't check events or outside, except for search menu */
  but = ui_region_find_active_but(region);

#ifdef USE_DRAG_POPUP

#  if defined(__APPLE__)
  constexpr int PopupTitleHoverCursor = WM_CURSOR_HAND;
  constexpr int PopupTitleDragCursor = WM_CURSOR_HAND_CLOSED;
#  else
  constexpr int PopupTitleHoverCursor = WM_CURSOR_MOVE;
  constexpr int PopupTitleDragCursor = WM_CURSOR_MOVE;
#  endif

  wmWindow *win = CTX_wm_window(C);

  if (!menu->is_grab && is_floating) {
    if (inside_title && (!but || but->type == ButType::Image)) {
      if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
        /* Initial press before starting to drag. */
        WM_cursor_set(win, PopupTitleDragCursor);
      }
      else if (event->type == MOUSEMOVE && !win->modalcursor) {
        /* Hover over draggable area. */
        WM_cursor_set(win, PopupTitleHoverCursor);
      }
    }
    else if (win->cursor == PopupTitleHoverCursor) {
      WM_cursor_set(win, WM_CURSOR_DEFAULT);
    }
  }

  if (menu->is_grab) {
    if (event->type == LEFTMOUSE) {
      menu->is_grab = false;
      WM_cursor_set(win, WM_CURSOR_DEFAULT);
      retval = WM_UI_HANDLER_BREAK;
    }
    else {
      if (event->type == MOUSEMOVE) {
        WM_cursor_set(win, PopupTitleDragCursor);
        blender::int2 mdiff = blender::int2(event->xy) - blender::int2(menu->grab_xy_prev);

        copy_v2_v2_int(menu->grab_xy_prev, event->xy);

        menu->popup_create_vars.event_xy += mdiff;

        ui_popup_translate(region, mdiff);
      }

      return retval;
    }
  }
#endif

  if (but && button_modal_state(but->active->state)) {
    if (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER)) {
      /* if a button is activated modal, always reset the start mouse
       * position of the towards mechanism to avoid losing focus,
       * and don't handle events */
      ui_mouse_motion_towards_reinit(menu, event->xy);
    }
  }
  else if (event->type == TIMER) {
    if (event->customdata == menu->scrolltimer) {
      ui_menu_scroll_to_y(region, block, my);
    }
  }
  else {
    /* for ui_mouse_motion_towards_block */
    if (event->type == MOUSEMOVE) {
      if (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER)) {
        ui_mouse_motion_towards_init(menu, event->xy);
      }

      /* add menu scroll timer, if needed */
      if (ui_menu_scroll_test(block, my)) {
        if (menu->scrolltimer == nullptr) {
          menu->scrolltimer = WM_event_timer_add(
              CTX_wm_manager(C), CTX_wm_window(C), TIMER, MENU_SCROLL_INTERVAL);
        }
      }
    }

    /* first block own event func */
    if (block->block_event_func && block->block_event_func(C, block, event)) {
      /* pass */
    } /* events not for active search menu button */
    else {
      int act = 0;

      switch (event->type) {

        /* Closing sub-levels of pull-downs.
         *
         * The actual event is handled by the button under the cursor.
         * This is done so we can right click on menu items even when they have sub-menus open.
         */
        case RIGHTMOUSE:
          if (inside == false) {
            if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP)) {
              if (block->saferct.first) {
                /* Currently right clicking on a top level pull-down (typically in the header)
                 * just closes the menu and doesn't support immediately handling the RMB event.
                 *
                 * To support we would need UI_RETURN_OUT_PARENT to be handled by
                 * top-level buttons, not just menus. Note that this isn't very important
                 * since it's easy to manually close these menus by clicking on them. */
                menu->menuretval = (level > 0 && is_parent_inside) ? UI_RETURN_OUT_PARENT :
                                                                     UI_RETURN_OUT;
              }
            }
            retval = WM_UI_HANDLER_BREAK;
          }
          break;

        /* Closing sub-levels of pull-downs. */
        case EVT_LEFTARROWKEY:
          if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP)) {
            if (block->saferct.first) {
              menu->menuretval = UI_RETURN_OUT;
            }
          }

          retval = WM_UI_HANDLER_BREAK;
          break;

        /* Opening sub-levels of pull-downs. */
        case EVT_RIGHTARROWKEY:
          if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP)) {

            if (ui_menu_pass_event_to_parent_if_nonactive(
                    menu, but, level, is_parent_menu, retval))
            {
              break;
            }

            but = ui_region_find_active_but(region);

            if (!but) {
              /* no item active, we make first active */
              if (block->direction & UI_DIR_UP) {
                but = ui_but_last(block);
              }
              else {
                but = ui_but_first(block);
              }
            }

            if (but && ELEM(but->type, ButType::Block, ButType::Pulldown)) {
              ui_handle_button_activate(C, region, but, BUTTON_ACTIVATE_OPEN);
            }
          }

          retval = WM_UI_HANDLER_BREAK;
          break;

        /* Smooth scrolling for popovers. */
        case MOUSEPAN: {
          if (event->modifier) {
            /* pass */
          }
          else if (!ui_block_is_menu(block)) {
            if (block->flag & (UI_BLOCK_CLIPTOP | UI_BLOCK_CLIPBOTTOM)) {
              const float dy = event->xy[1] - event->prev_xy[1];
              if (dy != 0.0f) {
                ui_menu_scroll_apply_offset_y(region, block, dy);

                if (but) {
                  but->active->cancel = true;
                  button_activate_exit(C, but, but->active, false, false);
                }
                WM_event_add_mousemove(CTX_wm_window(C));
              }
            }
            break;
          }
          ATTR_FALLTHROUGH;
        }
        case WHEELUPMOUSE:
        case WHEELDOWNMOUSE: {
          if (event->modifier) {
            /* pass */
          }
          else if (!ui_block_is_menu(block)) {
            const int scroll_dir = (event->type == WHEELUPMOUSE) ? 1 : -1;
            if (ui_menu_scroll_step(region, block, scroll_dir)) {
              if (but) {
                but->active->cancel = true;
                button_activate_exit(C, but, but->active, false, false);
              }
              WM_event_add_mousemove(CTX_wm_window(C));
            }
            break;
          }
          ATTR_FALLTHROUGH;
        }
        case EVT_UPARROWKEY:
        case EVT_DOWNARROWKEY:
        case EVT_PAGEUPKEY:
        case EVT_PAGEDOWNKEY:
        case EVT_HOMEKEY:
        case EVT_ENDKEY:
          /* Arrow-keys: only handle for block_loop blocks. */
          if (event->modifier) {
            /* pass */
          }
          else if (inside || (block->flag & UI_BLOCK_LOOP)) {
            int type = event->type;
            int val = event->val;

            /* Convert pan to scroll-wheel. */
            if (type == MOUSEPAN) {
              ui_pan_to_scroll(event, &type, &val);
            }

            if (val == KM_PRESS) {
              /* Determine scroll operation. */
              uiMenuScrollType scrolltype;

              if (ELEM(type, EVT_PAGEUPKEY, EVT_HOMEKEY)) {
                scrolltype = MENU_SCROLL_TOP;
              }
              else if (ELEM(type, EVT_PAGEDOWNKEY, EVT_ENDKEY)) {
                scrolltype = MENU_SCROLL_BOTTOM;
              }
              else if (ELEM(type, EVT_UPARROWKEY, WHEELUPMOUSE)) {
                scrolltype = MENU_SCROLL_UP;
              }
              else {
                scrolltype = MENU_SCROLL_DOWN;
              }

              if (ui_menu_pass_event_to_parent_if_nonactive(
                      menu, but, level, is_parent_menu, retval))
              {
                break;
              }

#ifdef USE_KEYNAV_LIMIT
              ui_mouse_motion_keynav_init(&menu->keynav_state, event);
#endif

              but = ui_region_find_active_but(region);
              if (but) {
                /* Apply scroll operation. */
                if (scrolltype == MENU_SCROLL_DOWN) {
                  but = ui_but_next(but);
                }
                else if (scrolltype == MENU_SCROLL_UP) {
                  but = ui_but_prev(but);
                }
                else if (scrolltype == MENU_SCROLL_TOP) {
                  but = ui_but_first(block);
                }
                else if (scrolltype == MENU_SCROLL_BOTTOM) {
                  but = ui_but_last(block);
                }
              }

              if (!but) {
                /* Wrap button or no active button. */
                uiBut *but_wrap = nullptr;
                if (ELEM(scrolltype, MENU_SCROLL_UP, MENU_SCROLL_BOTTOM)) {
                  but_wrap = ui_but_last(block);
                }
                else if (ELEM(scrolltype, MENU_SCROLL_DOWN, MENU_SCROLL_TOP)) {
                  but_wrap = ui_but_first(block);
                }
                if (but_wrap) {
                  but = but_wrap;
                }
              }

              if (but) {
                ui_handle_button_activate(C, region, but, BUTTON_ACTIVATE);
                ui_menu_scroll_to_but(region, block, but);
              }
            }

            retval = WM_UI_HANDLER_BREAK;
          }

          break;

        case EVT_ONEKEY:
        case EVT_PAD1:
          act = 1;
          ATTR_FALLTHROUGH;
        case EVT_TWOKEY:
        case EVT_PAD2:
          if (act == 0) {
            act = 2;
          }
          ATTR_FALLTHROUGH;
        case EVT_THREEKEY:
        case EVT_PAD3:
          if (act == 0) {
            act = 3;
          }
          ATTR_FALLTHROUGH;
        case EVT_FOURKEY:
        case EVT_PAD4:
          if (act == 0) {
            act = 4;
          }
          ATTR_FALLTHROUGH;
        case EVT_FIVEKEY:
        case EVT_PAD5:
          if (act == 0) {
            act = 5;
          }
          ATTR_FALLTHROUGH;
        case EVT_SIXKEY:
        case EVT_PAD6:
          if (act == 0) {
            act = 6;
          }
          ATTR_FALLTHROUGH;
        case EVT_SEVENKEY:
        case EVT_PAD7:
          if (act == 0) {
            act = 7;
          }
          ATTR_FALLTHROUGH;
        case EVT_EIGHTKEY:
        case EVT_PAD8:
          if (act == 0) {
            act = 8;
          }
          ATTR_FALLTHROUGH;
        case EVT_NINEKEY:
        case EVT_PAD9:
          if (act == 0) {
            act = 9;
          }
          ATTR_FALLTHROUGH;
        case EVT_ZEROKEY:
        case EVT_PAD0:
          if (act == 0) {
            act = 10;
          }

          if ((block->flag & UI_BLOCK_NUMSELECT) && event->val == KM_PRESS) {
            int count;

            if (ui_menu_pass_event_to_parent_if_nonactive(
                    menu, but, level, is_parent_menu, retval))
            {
              break;
            }

            /* Only respond to explicit press to avoid the event that opened the menu
             * activating an item when the key is held. */
            if (event->flag & WM_EVENT_IS_REPEAT) {
              break;
            }

            if (event->modifier & KM_ALT) {
              act += 10;
            }

            count = 0;
            for (const std::unique_ptr<uiBut> &but : block->buttons) {
              bool doit = false;

              if (!ELEM(
                      but->type, ButType::Label, ButType::Sepr, ButType::SeprLine, ButType::Image))
              {
                count++;
              }

              /* exception for rna layer buts */
              if (but->rnapoin.data && but->rnaprop &&
                  ELEM(RNA_property_subtype(but->rnaprop), PROP_LAYER, PROP_LAYER_MEMBER))
              {
                if (but->rnaindex == act - 1) {
                  doit = true;
                }
              }
              else if (ELEM(but->type,
                            ButType::But,
                            ButType::ButMenu,
                            ButType::Menu,
                            ButType::Block,
                            ButType::Pulldown) &&
                       count == act)
              {
                doit = true;
              }

              if (!(but->flag & UI_BUT_DISABLED) && doit) {
                /* activate buttons but open menu's */
                uiButtonActivateType activate;
                if (but->type == ButType::Pulldown) {
                  activate = BUTTON_ACTIVATE_OPEN;
                }
                else {
                  activate = BUTTON_ACTIVATE_APPLY;
                }

                ui_handle_button_activate(C, region, but.get(), activate);
                break;
              }
            }

            retval = WM_UI_HANDLER_BREAK;
          }
          break;

        /* Handle keystrokes on menu items */
        case EVT_AKEY:
        case EVT_BKEY:
        case EVT_CKEY:
        case EVT_DKEY:
        case EVT_EKEY:
        case EVT_FKEY:
        case EVT_GKEY:
        case EVT_HKEY:
        case EVT_IKEY:
        case EVT_JKEY:
        case EVT_KKEY:
        case EVT_LKEY:
        case EVT_MKEY:
        case EVT_NKEY:
        case EVT_OKEY:
        case EVT_PKEY:
        case EVT_QKEY:
        case EVT_RKEY:
        case EVT_SKEY:
        case EVT_TKEY:
        case EVT_UKEY:
        case EVT_VKEY:
        case EVT_WKEY:
        case EVT_XKEY:
        case EVT_YKEY:
        case EVT_ZKEY:
        case EVT_SPACEKEY: {
          if (ELEM(event->val, KM_PRESS, KM_DBL_CLICK) && ((event->modifier & ~KM_ALT) == 0) &&
              /* Only respond to explicit press to avoid the event that opened the menu
               * activating an item when the key is held. */
              (event->flag & WM_EVENT_IS_REPEAT) == 0)
          {

            /* Menu search if space-bar or #MenuTypeFlag::SearchOnKeyPress. */
            MenuType *mt = WM_menutype_find(menu->menu_idname, true);
            if ((mt && flag_is_set(mt->flag, MenuTypeFlag::SearchOnKeyPress)) ||
                event->type == EVT_SPACEKEY)
            {
              if ((level != 0) && (but == nullptr || !menu->menu_idname[0])) {
                /* Search parent if the child is open but not activated or not searchable. */
                menu->menuretval = UI_RETURN_OUT | UI_RETURN_OUT_PARENT;
              }
              else {
                retval = ui_handle_menu_letter_press_search(menu, event);
              }
              break;
            }

            if (ui_menu_pass_event_to_parent_if_nonactive(
                    menu, but, level, is_parent_menu, retval))
            {
              break;
            }

            /* Accelerator keys that allow "pressing" a menu entry by pressing a single key. */
            for (const std::unique_ptr<uiBut> &but_iter : block->buttons) {
              if (!(but_iter->flag & UI_BUT_DISABLED) && but_iter->menu_key == event->type) {
                if (ELEM(but_iter->type, ButType::But, ButType::IconToggle, ButType::IconToggleN))
                {
                  UI_but_execute(C, region, but_iter.get());
                }
                else {
                  ui_handle_button_activate_by_type(C, region, but_iter.get());
                }
                return WM_UI_HANDLER_BREAK;
              }
            }
          }
          default: {
            break;
          }
        }
      }
    }

    /* here we check return conditions for menus */
    if (block->flag & UI_BLOCK_LOOP) {
      /* If we click outside the block, verify if we clicked on the
       * button that opened us, otherwise we need to close,
       *
       * note that there is an exception for root level menus and
       * popups which you can click again to close.
       *
       * Events handled above may have already set the return value,
       * don't overwrite them, see: #61015.
       */
      if ((inside == false) && (menu->menuretval == 0)) {
        uiSafetyRct *saferct = static_cast<uiSafetyRct *>(block->saferct.first);

        if (event->type == MOUSEMOVE) {
          WM_tooltip_clear(C, win);
        }

        if (ELEM(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE)) {
          if (ELEM(event->val, KM_PRESS, KM_DBL_CLICK)) {
            if ((is_parent_menu == false) && (U.uiflag & USER_MENUOPENAUTO) == 0) {
              /* for root menus, allow clicking to close */
              if (block->flag & UI_BLOCK_OUT_1) {
                menu->menuretval = UI_RETURN_OK;
              }
              else {
                menu->menuretval = UI_RETURN_OUT;
              }
            }
            else if (saferct && !BLI_rctf_isect_pt(
                                    &saferct->parent, float(event->xy[0]), float(event->xy[1])))
            {
              if (block->flag & UI_BLOCK_OUT_1) {
                menu->menuretval = UI_RETURN_OK;
              }
              else {
                menu->menuretval = (U.flag & USER_MENU_CLOSE_LEAVE) ? UI_RETURN_OUT :
                                                                      UI_RETURN_CANCEL;
              }
            }
          }
          else if (ELEM(event->val, KM_RELEASE, KM_CLICK)) {
            /* For buttons that use a hold function,
             * exit when mouse-up outside the menu. */
            if (block->flag & UI_BLOCK_POPUP_HOLD) {
              /* NOTE: we could check the cursor is over the parent button. */
              menu->menuretval = UI_RETURN_CANCEL;
              retval = WM_UI_HANDLER_CONTINUE;
            }
          }
        }
      }

      if (menu->menuretval) {
        /* pass */
      }
#ifdef USE_KEYNAV_LIMIT
      else if ((event->type == MOUSEMOVE) &&
               ui_mouse_motion_keynav_test(&menu->keynav_state, event))
      {
        /* Don't handle the mouse-move if we're using key-navigation. */
        retval = WM_UI_HANDLER_BREAK;
      }
#endif
      else if (event->type == EVT_ESCKEY && event->val == KM_PRESS) {
        /* Escape cancels this and all preceding menus. */
        menu->menuretval = UI_RETURN_CANCEL;
      }
      else if (ELEM(event->type, EVT_RETKEY, EVT_PADENTER) && event->val == KM_PRESS) {
        uiBut *but_default = ui_region_find_first_but_test_flag(
            region, UI_BUT_ACTIVE_DEFAULT, UI_HIDDEN);
        if ((but_default != nullptr) && (but_default->active == nullptr)) {
          if (but_default->type == ButType::But) {
            UI_but_execute(C, region, but_default);
            retval = WM_UI_HANDLER_BREAK;
          }
          else {
            ui_handle_button_activate_by_type(C, region, but_default);
          }
        }
        else {
          uiBut *but_active = ui_region_find_active_but(region);

          /* enter will always close this block, we let the event
           * get handled by the button if it is activated, otherwise we cancel */
          if (but_active == nullptr) {
            menu->menuretval = UI_RETURN_CANCEL | UI_RETURN_POPUP_OK;
          }
        }
      }
#ifdef USE_DRAG_POPUP
      else if ((event->type == LEFTMOUSE) && (event->val == KM_PRESS) &&
               (inside && is_floating && inside_title))
      {
        if (!but || but->type == ButType::Image ||
            !ui_but_contains_point_px(but, region, event->xy))
        {
          if (but) {
            UI_but_tooltip_timer_remove(C, but);
          }

          menu->is_grab = true;
          copy_v2_v2_int(menu->grab_xy_prev, event->xy);
          retval = WM_UI_HANDLER_BREAK;
        }
      }
#endif
      else {

        /* check mouse moving outside of the menu */
        if (inside == false && (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER))) {
          uiSafetyRct *saferct;

          ui_mouse_motion_towards_check(block, menu, event->xy, is_parent_inside == false);

          /* Check for all parent rects, enables arrow-keys to be used. */
          for (saferct = static_cast<uiSafetyRct *>(block->saferct.first); saferct;
               saferct = saferct->next)
          {
            /* for mouse move we only check our own rect, for other
             * events we check all preceding block rects too to make
             * arrow keys navigation work */
            if (event->type != MOUSEMOVE ||
                saferct == static_cast<uiSafetyRct *>(block->saferct.first))
            {
              if (BLI_rctf_isect_pt(&saferct->parent, float(event->xy[0]), float(event->xy[1]))) {
                break;
              }
              if (BLI_rctf_isect_pt(&saferct->safety, float(event->xy[0]), float(event->xy[1]))) {
                break;
              }
            }
          }

          /* strict check, and include the parent rect */
          if (!menu->dotowards && !saferct && ((U.flag & USER_MENU_CLOSE_LEAVE) || level > 0)) {
            if (block->flag & UI_BLOCK_OUT_1) {
              menu->menuretval = UI_RETURN_OK;
            }
            else {
              menu->menuretval = UI_RETURN_OUT;
            }
          }
          else if (menu->dotowards && event->type == MOUSEMOVE) {
            retval = WM_UI_HANDLER_BREAK;
          }
        }
      }

      /* end switch */
    }
  }

  /* if we are didn't handle the event yet, lets pass it on to
   * buttons inside this region. disabled inside check .. not sure
   * anymore why it was there? but it meant enter didn't work
   * for example when mouse was not over submenu */
  if ((event->type == TIMER) ||
      (/* inside && */ (!menu->menuretval || (menu->menuretval & UI_RETURN_UPDATE)) &&
       retval == WM_UI_HANDLER_CONTINUE))
  {
    retval = ui_handle_menu_button(C, event, menu);
  }

#ifdef USE_UI_POPOVER_ONCE
  if (block->flag & UI_BLOCK_POPOVER_ONCE) {
    if ((event->type == LEFTMOUSE) && (event->val == KM_RELEASE)) {
      UI_popover_once_clear(static_cast<uiPopover *>(menu->popup_create_vars.arg));
      block->flag &= ~UI_BLOCK_POPOVER_ONCE;
    }
  }
#endif

  /* Don't handle double click events, re-handle as regular press/release. */
  if (retval == WM_UI_HANDLER_CONTINUE && event->val == KM_DBL_CLICK) {
    return retval;
  }

  /* if we set a menu return value, ensure we continue passing this on to
   * lower menus and buttons, so always set continue then, and if we are
   * inside the region otherwise, ensure we swallow the event */
  if (menu->menuretval) {
    return WM_UI_HANDLER_CONTINUE;
  }
  if (inside) {
    return WM_UI_HANDLER_BREAK;
  }
  return retval;
}

static int ui_handle_menu_return_submenu(bContext *C,
                                         const wmEvent *event,
                                         uiPopupBlockHandle *menu)
{
  ARegion *region = menu->region;
  uiBlock *block = static_cast<uiBlock *>(region->runtime->uiblocks.first);

  uiBut *but = ui_region_find_active_but(region);

  BLI_assert(but);

  uiHandleButtonData *data = but->active;
  uiPopupBlockHandle *submenu = data->menu;

  if (submenu->menuretval) {
    bool update;

    /* first decide if we want to close our own menu cascading, if
     * so pass on the sub menu return value to our own menu handle */
    if ((submenu->menuretval & UI_RETURN_OK) || (submenu->menuretval & UI_RETURN_CANCEL)) {
      if (!(block->flag & UI_BLOCK_KEEP_OPEN)) {
        menu->menuretval = submenu->menuretval;
        menu->butretval = data->retval;
      }
    }

    update = (submenu->menuretval & UI_RETURN_UPDATE) != 0;

    /* now let activated button in this menu exit, which
     * will actually close the submenu too */
    ui_handle_button_return_submenu(C, event, but);

    if (update) {
      submenu->menuretval = 0;
    }
  }

  if (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER)) {
    /* for cases where close does not cascade, allow the user to
     * move the mouse back towards the menu without closing */
    ui_mouse_motion_towards_reinit(menu, event->xy);
  }

  if (menu->menuretval) {
    return WM_UI_HANDLER_CONTINUE;
  }
  return WM_UI_HANDLER_BREAK;
}

static bool ui_but_pie_menu_supported_apply(uiBut *but)
{
  return !ELEM(but->type, ButType::NumSlider, ButType::Num);
}

static int ui_but_pie_menu_apply(bContext *C,
                                 uiPopupBlockHandle *menu,
                                 uiBut *but,
                                 bool force_close)
{
  const int retval = WM_UI_HANDLER_BREAK;

  if (but && ui_but_pie_menu_supported_apply(but)) {
    if (but->type == ButType::Menu) {
      /* forcing the pie menu to close will not handle menus */
      if (!force_close) {
        uiBut *active_but = ui_region_find_active_but(menu->region);

        if (active_but) {
          button_activate_exit(C, active_but, active_but->active, false, false);
        }

        button_activate_init(C, menu->region, but, BUTTON_ACTIVATE_OPEN);
        return retval;
      }
      menu->menuretval = UI_RETURN_CANCEL;
    }
    else {
      button_activate_exit(C, but, but->active, false, false);

      menu->menuretval = UI_RETURN_OK;
    }
  }
  else {
    menu->menuretval = UI_RETURN_CANCEL;

    ED_region_tag_redraw(menu->region);
  }

  return retval;
}

static uiBut *ui_block_pie_dir_activate(uiBlock *block, const wmEvent *event, RadialDirection dir)
{
  if ((block->flag & UI_BLOCK_NUMSELECT) && event->val == KM_PRESS) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but->pie_dir == dir && !ELEM(but->type, ButType::Sepr, ButType::SeprLine)) {
        return but.get();
      }
    }
  }

  return nullptr;
}

static int ui_but_pie_button_activate(bContext *C, uiBut *but, uiPopupBlockHandle *menu)
{
  if (but == nullptr) {
    return WM_UI_HANDLER_BREAK;
  }

  uiBut *active_but = ui_region_find_active_but(menu->region);

  if (active_but) {
    /* Use onfree to not execute the hovered active_but. */
    button_activate_exit(C, active_but, active_but->active, false, true);
  }

  button_activate_init(C, menu->region, but, BUTTON_ACTIVATE_OVER);
  return ui_but_pie_menu_apply(C, menu, but, false);
}

static int ui_pie_handler(bContext *C, const wmEvent *event, uiPopupBlockHandle *menu)
{
  /* we block all events, this is modal interaction,
   * except for drop events which is described below */
  int retval = WM_UI_HANDLER_BREAK;

  if (event->type == EVT_DROP) {
    /* may want to leave this here for later if we support pie ovens */

    retval = WM_UI_HANDLER_CONTINUE;
  }

  ARegion *region = menu->region;
  uiBlock *block = static_cast<uiBlock *>(region->runtime->uiblocks.first);

  const bool is_click_style = (block->pie_data.flags & UI_PIE_CLICK_STYLE);

  /* if there's an active modal button, don't check events or outside, except for search menu */
  uiBut *but_active = ui_region_find_active_but(region);

  if (menu->scrolltimer == nullptr) {
    menu->scrolltimer = WM_event_timer_add(
        CTX_wm_manager(C), CTX_wm_window(C), TIMER, PIE_MENU_INTERVAL);
    menu->scrolltimer->time_duration = 0.0;
  }

  const double duration = menu->scrolltimer->time_duration;

  float event_xy[2] = {float(event->xy[0]), float(event->xy[1])};

  ui_window_to_block_fl(region, block, &event_xy[0], &event_xy[1]);

  /* Distance from initial point. */
  const float dist = ui_block_calc_pie_segment(block, event_xy);

  if (but_active && button_modal_state(but_active->active->state)) {
    retval = ui_handle_menu_button(C, event, menu);
  }
  else {
    if (event->type == TIMER) {
      if (event->customdata == menu->scrolltimer) {
        /* deactivate initial direction after a while */
        if (duration > 0.01 * U.pie_initial_timeout) {
          block->pie_data.flags &= ~UI_PIE_INITIAL_DIRECTION;
        }

        /* handle animation */
        if (!(block->pie_data.flags & UI_PIE_ANIMATION_FINISHED)) {
          const double final_time = (U.uiflag & USER_REDUCE_MOTION) ?
                                        0.0f :
                                        0.01 * U.pie_animation_timeout;
          float fac = duration / final_time;
          const float pie_radius = U.pie_menu_radius * UI_SCALE_FAC;

          if (fac > 1.0f) {
            fac = 1.0f;
            block->pie_data.flags |= UI_PIE_ANIMATION_FINISHED;
          }

          for (const std::unique_ptr<uiBut> &but : block->buttons) {
            if (but->pie_dir != UI_RADIAL_NONE) {
              float vec[2];
              float center[2];

              ui_but_pie_dir(but->pie_dir, vec);

              center[0] = (vec[0] > 0.01f) ? 0.5f : ((vec[0] < -0.01f) ? -0.5f : 0.0f);
              center[1] = (vec[1] > 0.99f) ? 0.5f : ((vec[1] < -0.99f) ? -0.5f : 0.0f);

              center[0] *= BLI_rctf_size_x(&but->rect);
              center[1] *= BLI_rctf_size_y(&but->rect);

              mul_v2_fl(vec, pie_radius);
              add_v2_v2(vec, center);
              mul_v2_fl(vec, fac);
              add_v2_v2(vec, block->pie_data.pie_center_spawned);

              BLI_rctf_recenter(&but->rect, vec[0], vec[1]);
            }
          }
          block->pie_data.alphafac = fac;

          ED_region_tag_redraw(region);
        }
      }

      /* Check pie velocity here if gesture has ended. */
      if (block->pie_data.flags & UI_PIE_GESTURE_END_WAIT) {
        float len_sq = 10;

        /* use a time threshold to ensure we leave time to the mouse to move */
        if (duration - block->pie_data.duration_gesture > 0.02) {
          len_sq = len_squared_v2v2(event_xy, block->pie_data.last_pos);
          copy_v2_v2(block->pie_data.last_pos, event_xy);
          block->pie_data.duration_gesture = duration;
        }

        if (len_sq < 1.0f) {
          uiBut *but = ui_region_find_active_but(menu->region);

          if (but) {
            return ui_but_pie_menu_apply(C, menu, but, true);
          }
        }
      }
    }

    if (event->type == block->pie_data.event_type && !is_click_style) {
      if (event->val != KM_RELEASE) {
        ui_handle_menu_button(C, event, menu);

        if (len_squared_v2v2(event_xy, block->pie_data.pie_center_init) > PIE_CLICK_THRESHOLD_SQ) {
          block->pie_data.flags |= UI_PIE_DRAG_STYLE;
        }
        /* why redraw here? It's simple, we are getting many double click events here.
         * Those operate like mouse move events almost */
        ED_region_tag_redraw(region);
      }
      else {
        if ((duration < 0.01 * U.pie_tap_timeout) && !(block->pie_data.flags & UI_PIE_DRAG_STYLE))
        {
          block->pie_data.flags |= UI_PIE_CLICK_STYLE;
        }
        else {
          uiBut *but = ui_region_find_active_but(menu->region);

          if (but && (U.pie_menu_confirm > 0) &&
              (dist >= UI_SCALE_FAC * (U.pie_menu_threshold + U.pie_menu_confirm)))
          {
            return ui_but_pie_menu_apply(C, menu, but, true);
          }

          retval = ui_but_pie_menu_apply(C, menu, but, true);
        }
      }
    }
    else {
      /* direction from numpad */
      RadialDirection num_dir = UI_RADIAL_NONE;

      switch (event->type) {
        case MOUSEMOVE:
          if (!is_click_style) {
            const float len_sq = len_squared_v2v2(event_xy, block->pie_data.pie_center_init);

            /* here we use the initial position explicitly */
            if (len_sq > PIE_CLICK_THRESHOLD_SQ) {
              block->pie_data.flags |= UI_PIE_DRAG_STYLE;
            }

            /* here instead, we use the offset location to account for the initial
             * direction timeout */
            if ((U.pie_menu_confirm > 0) &&
                (dist >= UI_SCALE_FAC * (U.pie_menu_threshold + U.pie_menu_confirm)))
            {
              block->pie_data.flags |= UI_PIE_GESTURE_END_WAIT;
              copy_v2_v2(block->pie_data.last_pos, event_xy);
              block->pie_data.duration_gesture = duration;
            }
          }

          ui_handle_menu_button(C, event, menu);

          /* mouse move should always refresh the area for pie menus */
          ED_region_tag_redraw(region);
          break;

        case LEFTMOUSE:
          if (is_click_style) {
            if (block->pie_data.flags & UI_PIE_INVALID_DIR) {
              menu->menuretval = UI_RETURN_CANCEL;
            }
            else {
              retval = ui_handle_menu_button(C, event, menu);
            }
          }
          break;

        case WINDEACTIVATE: {
          /* Prevent waiting for the pie key release if it was released outside of focus. */
          wmWindow *win = CTX_wm_window(C);
          if (win) {
            win->pie_event_type_lock = EVENT_NONE;
          }
          menu->menuretval = UI_RETURN_CANCEL;
          break;
        }

        case EVT_ESCKEY:
        case RIGHTMOUSE:
          menu->menuretval = UI_RETURN_CANCEL;
          break;

        case EVT_AKEY:
        case EVT_BKEY:
        case EVT_CKEY:
        case EVT_DKEY:
        case EVT_EKEY:
        case EVT_FKEY:
        case EVT_GKEY:
        case EVT_HKEY:
        case EVT_IKEY:
        case EVT_JKEY:
        case EVT_KKEY:
        case EVT_LKEY:
        case EVT_MKEY:
        case EVT_NKEY:
        case EVT_OKEY:
        case EVT_PKEY:
        case EVT_QKEY:
        case EVT_RKEY:
        case EVT_SKEY:
        case EVT_TKEY:
        case EVT_UKEY:
        case EVT_VKEY:
        case EVT_WKEY:
        case EVT_XKEY:
        case EVT_YKEY:
        case EVT_ZKEY: {
          if (ELEM(event->val, KM_PRESS, KM_DBL_CLICK) && ((event->modifier & ~KM_ALT) == 0)) {
            for (const std::unique_ptr<uiBut> &but : block->buttons) {
              if (but->menu_key == event->type) {
                ui_but_pie_button_activate(C, but.get(), menu);
              }
            }
          }
          break;
        }

#define CASE_NUM_TO_DIR(n, d) \
  case (EVT_ZEROKEY + n): \
  case (EVT_PAD0 + n): { \
    if (num_dir == UI_RADIAL_NONE) { \
      num_dir = d; \
    } \
  } \
    (void)0

          CASE_NUM_TO_DIR(1, UI_RADIAL_SW);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(2, UI_RADIAL_S);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(3, UI_RADIAL_SE);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(4, UI_RADIAL_W);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(6, UI_RADIAL_E);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(7, UI_RADIAL_NW);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(8, UI_RADIAL_N);
          ATTR_FALLTHROUGH;
          CASE_NUM_TO_DIR(9, UI_RADIAL_NE);
          {
            uiBut *but = ui_block_pie_dir_activate(block, event, num_dir);
            retval = ui_but_pie_button_activate(C, but, menu);
            break;
          }
#undef CASE_NUM_TO_DIR
        default:
          retval = ui_handle_menu_button(C, event, menu);
          break;
      }
    }
  }

  return retval;
}

static int ui_handle_menus_recursive(bContext *C,
                                     const wmEvent *event,
                                     uiPopupBlockHandle *menu,
                                     int level,
                                     const bool is_parent_inside,
                                     const bool is_parent_menu,
                                     const bool is_floating)
{
  int retval = WM_UI_HANDLER_CONTINUE;
  bool do_towards_reinit = false;

  /* check if we have a submenu, and handle events for it first */
  uiBut *but = ui_region_find_active_but(menu->region);
  uiHandleButtonData *data = (but) ? but->active : nullptr;
  uiPopupBlockHandle *submenu = (data) ? data->menu : nullptr;

  if (submenu) {
    uiBlock *block = static_cast<uiBlock *>(menu->region->runtime->uiblocks.first);
    const bool is_menu = ui_block_is_menu(block);
    bool inside = false;
    /* root pie menus accept the key that spawned
     * them as double click to improve responsiveness */
    const bool do_recursion = (!(block->flag & UI_BLOCK_PIE_MENU) ||
                               event->type != block->pie_data.event_type);

    if (do_recursion) {
      if (is_parent_inside == false) {
        int mx = event->xy[0];
        int my = event->xy[1];
        ui_window_to_block(menu->region, block, &mx, &my);
        inside = BLI_rctf_isect_pt(&block->rect, mx, my);
      }

      retval = ui_handle_menus_recursive(
          C, event, submenu, level + 1, is_parent_inside || inside, is_menu, false);
    }
  }
  else if (!but && event->val == KM_PRESS && event->type == LEFTMOUSE) {
    LISTBASE_FOREACH (uiBlock *, block, &menu->region->runtime->uiblocks) {
      if (block->panel) {
        int mx = event->xy[0];
        int my = event->xy[1];
        ui_window_to_block(menu->region, block, &mx, &my);
        if (!IN_RANGE(float(mx), block->rect.xmin, block->rect.xmax)) {
          break;
        }
        LayoutPanelHeader *header = ui_layout_panel_header_under_mouse(*block->panel, my);
        if (header) {
          ED_region_tag_redraw(menu->region);
          ED_region_tag_refresh_ui(menu->region);
          ARegion *prev_region_popup = CTX_wm_region_popup(C);
          /* Set the current context popup region so the handler context can access to it. */
          CTX_wm_region_popup_set(C, menu->region);
          ui_panel_drag_collapse_handler_add(C, !ui_layout_panel_toggle_open(C, header));
          /* Restore previous popup region. */
          CTX_wm_region_popup_set(C, prev_region_popup);
          retval = WM_UI_HANDLER_BREAK;
        }
      }
    }
  }

  /* now handle events for our own menu */

  if (retval == WM_UI_HANDLER_CONTINUE) {
    retval = ui_handle_region_semi_modal_buttons(C, event, menu->region);
  }

  if (retval == WM_UI_HANDLER_CONTINUE || event->type == TIMER) {
    const bool do_but_search = (but && (but->type == ButType::SearchMenu));
    if (submenu && submenu->menuretval) {
      const bool do_ret_out_parent = (submenu->menuretval & UI_RETURN_OUT_PARENT) != 0;
      retval = ui_handle_menu_return_submenu(C, event, menu);
      submenu = nullptr; /* hint not to use this, it may be freed by call above */
      (void)submenu;
      /* we may want to quit the submenu and handle the even in this menu,
       * if its important to use it, check 'data->menu' first */
      if (((retval == WM_UI_HANDLER_BREAK) && do_ret_out_parent) == false) {
        /* skip applying the event */
        return retval;
      }
    }

    if (do_but_search) {
      uiBlock *block = static_cast<uiBlock *>(menu->region->runtime->uiblocks.first);

      retval = ui_handle_menu_button(C, event, menu);

      if (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER)) {
        /* when there is a active search button and we close it,
         * we need to reinit the mouse coords #35346. */
        if (ui_region_find_active_but(menu->region) != but) {
          do_towards_reinit = true;
        }
      }
    }
    else {
      uiBlock *block = static_cast<uiBlock *>(menu->region->runtime->uiblocks.first);

      if (block->flag & UI_BLOCK_PIE_MENU) {
        retval = ui_pie_handler(C, event, menu);
      }
      else if (event->type == LEFTMOUSE || event->val != KM_DBL_CLICK) {
        bool handled = false;

        if (uiBut *listbox = ui_list_find_mouse_over(menu->region, event)) {
          const int retval_test = ui_handle_list_event(C, event, menu->region, listbox);
          if (retval_test != WM_UI_HANDLER_CONTINUE) {
            retval = retval_test;
            handled = true;
          }
        }

        if (handled == false) {
          retval = ui_handle_menu_event(
              C, event, menu, level, is_parent_inside, is_parent_menu, is_floating);
        }
      }
    }
  }

  if (!menu->retvalue) {
    ui_handle_viewlist_items_hover(event, menu->region);
  }
  /* Handle mouse clicks on overlapping view item button. */
  ui_handle_view_item_event(C, event, but, menu->region);

  if (do_towards_reinit) {
    ui_mouse_motion_towards_reinit(menu, event->xy);
  }

  return retval;
}

void UI_popup_menu_retval_set(const uiBlock *block, const int retval, const bool enable)
{
  uiPopupBlockHandle *menu = block->handle;
  if (menu) {
    menu->menuretval = enable ? (menu->menuretval | retval) : (menu->menuretval & retval);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Event Handlers
 * \{ */

static int ui_region_handler(bContext *C, const wmEvent *event, void * /*userdata*/)
{
  /* here we handle buttons at the region level, non-modal */
  ARegion *region = CTX_wm_region(C);
  int retval = WM_UI_HANDLER_CONTINUE;

  if (region == nullptr || BLI_listbase_is_empty(&region->runtime->uiblocks)) {
    return retval;
  }

  /* either handle events for already activated button or try to activate */
  uiBut *but = ui_region_find_active_but(region);
  uiBut *listbox = ui_list_find_mouse_over(region, event);

  retval = ui_handler_panel_region(C, event, region, listbox ? listbox : but);

  if (retval == WM_UI_HANDLER_CONTINUE && listbox) {
    retval = ui_handle_list_event(C, event, region, listbox);

    /* interactions with the listbox should disable tips */
    if (retval == WM_UI_HANDLER_BREAK) {
      if (but) {
        UI_but_tooltip_timer_remove(C, but);
      }
    }
  }

  if (retval == WM_UI_HANDLER_CONTINUE) {
    retval = ui_handle_region_semi_modal_buttons(C, event, region);
  }

  if (retval == WM_UI_HANDLER_CONTINUE) {
    if (but) {
      retval = ui_handle_button_event(C, event, but);
    }
    else {
      retval = ui_handle_button_over(C, event, region);
    }
  }

  /* Re-enable tool-tips. */
  if (event->type == MOUSEMOVE &&
      (event->xy[0] != event->prev_xy[0] || event->xy[1] != event->prev_xy[1]))
  {
    ui_blocks_set_tooltips(region, true);
  }

  /* Always do this, to reliably update view and UI-list item highlighting, even if
   * the mouse hovers a button nested in the item (it's an overlapping layout). */
  ui_handle_viewlist_items_hover(event, region);
  if (retval == WM_UI_HANDLER_CONTINUE) {
    retval = ui_handle_view_item_event(C, event, but, region);
  }

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);

  return retval;
}

static void ui_region_handler_remove(bContext *C, void * /*userdata*/)
{
  ARegion *region = CTX_wm_region(C);
  if (region == nullptr) {
    return;
  }

  UI_blocklist_free(C, region);
  bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return;
  }

  /* delayed apply callbacks, but not for screen level regions, those
   * we rather do at the very end after closing them all, which will
   * be done in ui_region_handler/window */
  if (BLI_findindex(&screen->regionbase, region) == -1) {
    ui_apply_but_funcs_after(C);
  }
}

static int ui_handle_region_semi_modal_buttons(bContext *C, const wmEvent *event, ARegion *region)
{
  /* If there's a fully modal button, it has priority. */
  if (const uiBut *active_but = ui_region_find_active_but(region)) {
    BLI_assert(active_but->semi_modal_state == nullptr);
    if (button_modal_state(active_but->active->state)) {
      return WM_UI_HANDLER_CONTINUE;
    }
  }

  int retval = WM_UI_HANDLER_CONTINUE;

  foreach_semi_modal_but_as_active(C, region, [&](uiBut *semi_modal_but) {
    if (retval == WM_UI_HANDLER_CONTINUE) {
      retval = ui_handle_button_event(C, event, semi_modal_but);
    }
  });

  return retval;
}

/* Return true if we should open another menu while one is already open. */
static bool ui_can_activate_other_menu(uiBut *but, uiBut *but_other, const wmEvent *event)
{
  if (but == but_other || but_other->flag & UI_BUT_DISABLED || but_other->menu_no_hover_open) {
    return false;
  }

  if (!ELEM(but_other->type, ButType::Pulldown, ButType::Popover, ButType::Menu)) {
    return false;
  }

  if (!ELEM(but->type, ButType::Pulldown, ButType::Popover, ButType::Menu)) {
    return false;
  }

  uiHandleButtonData *data = but->active;
  if (!(data->menu->direction & (UI_DIR_DOWN | UI_DIR_UP))) {
    return true;
  }

  if (data->menu && data->menu->region &&
      (BLI_rcti_size_x(&data->menu->region->winrct) > (600.0f * UI_SCALE_FAC)))
  {
    /* If the open menu is super wide then don't switch to any neighbors. */
    return false;
  }

  float safety = 4.0f * UI_SCALE_FAC;
  if (!but_other->str.empty()) {
    safety += 4.0f * UI_SCALE_FAC;
  }

  float left, right;
  if (but_other->rect.xmin < but->rect.xmin) {
    /* Right to Left. */
    if (but->rect.xmin - but_other->rect.xmax > (24.0f * UI_SCALE_FAC)) {
      /* If they are far enough part just switch. */
      return true;
    }
    right = but->rect.xmax;
    left = but_other->rect.xmax;
    if (ELEM(but_other->type, ButType::Popover, ButType::Menu)) {
      /* Skip the drop-down arrow on the right of it. */
      safety += 8.0f * UI_SCALE_FAC;
    }
    left -= safety;
  }
  else {
    /* Left to Right. */
    if (but_other->rect.xmin - but->rect.xmax > (24.0f * UI_SCALE_FAC)) {
      /* If they are far enough part just switch. */
      return true;
    }
    left = but->rect.xmin;
    right = but_other->rect.xmin;
    if (but_other->icon && !but_other->str.empty()) {
      /* Skip the icon on the left of it. */
      safety += 16.0f * UI_SCALE_FAC;
    }
    right += safety;
  }

  return (event->mval[0] < int(left) || event->mval[0] > int(right));
}

/* handle buttons at the window level, modal, for example while
 * number sliding, text editing, or when a menu block is open */
static int ui_handler_region_menu(bContext *C, const wmEvent *event, void * /*userdata*/)
{
  ARegion *region_popup = CTX_wm_region_popup(C);
  ARegion *region = region_popup ? region_popup : CTX_wm_region(C);
  int retval = WM_UI_HANDLER_CONTINUE;

  uiBut *but = ui_region_find_active_but(region);

  if (but) {
    bScreen *screen = CTX_wm_screen(C);
    uiBut *but_other;

    /* handle activated button events */
    uiHandleButtonData *data = but->active;

    if ((data->state == BUTTON_STATE_MENU_OPEN) &&
        /* Make sure this popup isn't dragging a button.
         * can happen with popovers (see #67882). */
        (ui_region_find_active_but(data->menu->region) == nullptr) &&
        /* make sure mouse isn't inside another menu (see #43247) */
        (ui_screen_region_find_mouse_over(screen, event) == nullptr) &&
        (but_other = UI_but_find_mouse_over(region, event)) &&
        ui_can_activate_other_menu(but, but_other, event) &&
        /* Hover-opening menu's doesn't work well for buttons over one another
         * along the same axis the menu is opening on (see #71719). */
        (((data->menu->direction & (UI_DIR_LEFT | UI_DIR_RIGHT)) &&
          BLI_rctf_isect_rect_x(&but->rect, &but_other->rect, nullptr)) ||
         ((data->menu->direction & (UI_DIR_DOWN | UI_DIR_UP)) &&
          BLI_rctf_isect_rect_y(&but->rect, &but_other->rect, nullptr))))
    {
      /* if mouse moves to a different root-level menu button,
       * open it to replace the current menu */
      ui_handle_button_activate(C, region, but_other, BUTTON_ACTIVATE_OVER);
      button_activate_state(C, but_other, BUTTON_STATE_MENU_OPEN);
      retval = WM_UI_HANDLER_BREAK;
    }
    else if (data->state == BUTTON_STATE_MENU_OPEN) {
      /* handle events for menus and their buttons recursively,
       * this will handle events from the top to the bottom menu */
      if (data->menu) {
        retval = ui_handle_menus_recursive(C, event, data->menu, 0, false, false, false);
      }

      /* handle events for the activated button */
      if ((data->menu && (retval == WM_UI_HANDLER_CONTINUE)) || (event->type == TIMER)) {
        if (data->menu && data->menu->menuretval) {
          ui_handle_button_return_submenu(C, event, but);
          retval = WM_UI_HANDLER_BREAK;
        }
        else {
          retval = ui_handle_button_event(C, event, but);
        }
      }
    }
    else {
      /* handle events for the activated button */
      retval = ui_handle_button_event(C, event, but);
    }
  }

  /* Re-enable tool-tips. */
  if (event->type == MOUSEMOVE &&
      (event->xy[0] != event->prev_xy[0] || event->xy[1] != event->prev_xy[1]))
  {
    ui_blocks_set_tooltips(region, true);
  }

  if (but && but->active && but->active->menu) {
    /* Set correct context popup-region. The handling button above breaks if we set the region
     * first, so only set it for executing the #uiAfterFunc. */
    CTX_wm_region_popup_set(C, but->active->menu->region);
  }

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);

  /* Reset to previous context region. */
  CTX_wm_region_popup_set(C, region_popup);

  /* Don't handle double-click events,
   * these will be converted into regular clicks which we handle. */
  if (retval == WM_UI_HANDLER_CONTINUE) {
    if (event->val == KM_DBL_CLICK) {
      return WM_UI_HANDLER_CONTINUE;
    }
  }

  /* we block all events, this is modal interaction */
  return WM_UI_HANDLER_BREAK;
}

/* two types of popups, one with operator + enum, other with regular callbacks */
static int ui_popup_handler(bContext *C, const wmEvent *event, void *userdata)
{
  uiPopupBlockHandle *menu = static_cast<uiPopupBlockHandle *>(userdata);
  /* we block all events, this is modal interaction,
   * except for drop events which is described below */
  int retval = WM_UI_HANDLER_BREAK;
  bool reset_pie = false;

  ARegion *region_popup = CTX_wm_region_popup(C);
  CTX_wm_region_popup_set(C, menu->region);

  if (event->type == EVT_DROP || event->val == KM_DBL_CLICK) {
    /* EVT_DROP:
     *   If we're handling drop event we'll want it to be handled by popup callee as well,
     *   so it'll be possible to perform such operations as opening .blend files by dropping
     *   them into blender, even if there's opened popup like splash screen (sergey).
     * KM_DBL_CLICK:
     *   Continue in case of double click so wm_handlers_do calls handler again with KM_PRESS
     *   event. This is needed to ensure correct button handling for fast clicking (#47532).
     */

    retval = WM_UI_HANDLER_CONTINUE;
  }

  ui_handle_menus_recursive(C, event, menu, 0, false, false, true);

  /* free if done, does not free handle itself */
  if (menu->menuretval) {
    wmWindow *win = CTX_wm_window(C);
    /* copy values, we have to free first (closes region) */
    const uiPopupBlockHandle temp = *menu;
    uiBlock *block = static_cast<uiBlock *>(menu->region->runtime->uiblocks.first);

    /* set last pie event to allow chained pie spawning */
    if (block->flag & UI_BLOCK_PIE_MENU) {
      win->pie_event_type_last = block->pie_data.event_type;
      reset_pie = true;
    }

    ui_popup_block_free(C, menu);
    UI_popup_handlers_remove(&win->modalhandlers, menu);
    CTX_wm_region_popup_set(C, nullptr);

#ifdef USE_DRAG_TOGGLE
    {
      WM_event_free_ui_handler_all(C,
                                   &win->modalhandlers,
                                   ui_handler_region_drag_toggle,
                                   ui_handler_region_drag_toggle_remove);
    }
#endif

    if ((temp.menuretval & UI_RETURN_OK) || (temp.menuretval & UI_RETURN_POPUP_OK)) {
      if (temp.popup_func) {
        temp.popup_func(C, temp.popup_arg, temp.retvalue);
      }
    }
    else if (temp.cancel_func) {
      temp.cancel_func(C, temp.popup_arg);
    }

    WM_event_add_mousemove(win);
  }
  else {
    /* Re-enable tool-tips */
    if (event->type == MOUSEMOVE &&
        (event->xy[0] != event->prev_xy[0] || event->xy[1] != event->prev_xy[1]))
    {
      ui_blocks_set_tooltips(menu->region, true);
    }
  }

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);

  if (reset_pie) {
    /* Reacquire window in case pie invalidates it somehow. */
    wmWindow *win = CTX_wm_window(C);

    if (win) {
      win->pie_event_type_last = EVENT_NONE;
    }
  }

  CTX_wm_region_set(C, region_popup);

  return retval;
}

static void ui_popup_handler_remove(bContext *C, void *userdata)
{
  uiPopupBlockHandle *menu = static_cast<uiPopupBlockHandle *>(userdata);

  /* More correct would be to expect UI_RETURN_CANCEL here, but not wanting to
   * cancel when removing handlers because of file exit is a rare exception.
   * So instead of setting cancel flag for all menus before removing handlers,
   * just explicitly flag menu with UI_RETURN_OK to avoid canceling it. */
  if ((menu->menuretval & UI_RETURN_OK) == 0 && menu->cancel_func) {
    menu->cancel_func(C, menu->popup_arg);
  }

  /* free menu block if window is closed for some reason */
  ui_popup_block_free(C, menu);

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);
}

void UI_region_handlers_add(ListBase *handlers)
{
  WM_event_remove_ui_handler(
      handlers, ui_region_handler, ui_region_handler_remove, nullptr, false);
  WM_event_add_ui_handler(nullptr,
                          handlers,
                          ui_region_handler,
                          ui_region_handler_remove,
                          nullptr,
                          eWM_EventHandlerFlag(0));
}

void UI_popup_handlers_add(bContext *C,
                           ListBase *handlers,
                           uiPopupBlockHandle *popup,
                           const char flag)
{
  WM_event_add_ui_handler(
      C, handlers, ui_popup_handler, ui_popup_handler_remove, popup, eWM_EventHandlerFlag(flag));
}

void UI_popup_handlers_remove(ListBase *handlers, uiPopupBlockHandle *popup)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;

      if (handler->handle_fn == ui_popup_handler &&
          handler->remove_fn == ui_popup_handler_remove && handler->user_data == popup)
      {
        /* tag refresh parent popup */
        wmEventHandler_UI *handler_next = (wmEventHandler_UI *)handler->head.next;
        if (handler_next && handler_next->head.type == WM_HANDLER_TYPE_UI &&
            handler_next->handle_fn == ui_popup_handler &&
            handler_next->remove_fn == ui_popup_handler_remove)
        {
          uiPopupBlockHandle *parent_popup = static_cast<uiPopupBlockHandle *>(
              handler_next->user_data);
          ED_region_tag_refresh_ui(parent_popup->region);
        }
        break;
      }
    }
  }

  WM_event_remove_ui_handler(handlers, ui_popup_handler, ui_popup_handler_remove, popup, false);
}

void UI_popup_handlers_remove_all(bContext *C, ListBase *handlers)
{
  WM_event_free_ui_handler_all(C, handlers, ui_popup_handler, ui_popup_handler_remove);
}

bool UI_textbutton_activate_rna(const bContext *C,
                                ARegion *region,
                                const void *rna_poin_data,
                                const char *rna_prop_id)
{
  uiBlock *block_text = nullptr;
  uiBut *but_text = nullptr;

  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but->type == ButType::Text) {
        if (but->rnaprop && but->rnapoin.data == rna_poin_data) {
          if (STREQ(RNA_property_identifier(but->rnaprop), rna_prop_id)) {
            block_text = block;
            but_text = but.get();
            break;
          }
        }
      }
    }
    if (but_text) {
      break;
    }
  }

  if (but_text) {
    ARegion *region_ctx = CTX_wm_region(C);

    /* Temporary context override for activating the button. */
    CTX_wm_region_set(const_cast<bContext *>(C), region);
    UI_but_active_only(C, region, block_text, but_text);
    CTX_wm_region_set(const_cast<bContext *>(C), region_ctx);
    return true;
  }
  return false;
}

bool UI_textbutton_activate_but(const bContext *C, uiBut *actbut)
{
  ARegion *region = CTX_wm_region(C);
  uiBlock *block_text = nullptr;
  uiBut *but_text = nullptr;

  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but.get() == actbut && but->type == ButType::Text) {
        block_text = block;
        but_text = but.get();
        break;
      }
    }

    if (but_text) {
      break;
    }
  }

  if (but_text) {
    UI_but_active_only(C, region, block_text, but_text);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Utilities
 * \{ */

void UI_region_free_active_but_all(bContext *C, ARegion *region)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but->active == nullptr) {
        continue;
      }
      ui_but_active_free(C, but.get());
    }
  }
}

void UI_screen_free_active_but_highlight(const bContext *C, bScreen *screen)
{
  wmWindow *win = CTX_wm_window(C);

  ED_screen_areas_iter (win, screen, area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      uiBut *but = ui_region_find_active_but(region);
      if (but) {
        uiHandleButtonData *data = but->active;

        if (data->menu == nullptr && data->searchbox == nullptr) {
          if (data->state == BUTTON_STATE_HIGHLIGHT) {
            ui_but_active_free(C, but);
          }
        }
      }
    }
  }
}

uiBut *UI_but_active_drop_name_button(const bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  uiBut *but = ui_region_find_active_but(region);

  if (but) {
    if (ELEM(but->type, ButType::Text, ButType::SearchMenu)) {
      return but;
    }
  }

  return nullptr;
}

bool UI_but_active_drop_name(const bContext *C)
{
  return UI_but_active_drop_name_button(C) != nullptr;
}

bool UI_but_active_drop_color(bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  if (region) {
    uiBut *but = ui_region_find_active_but(region);

    if (but && but->type == ButType::Color) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Block Interaction API
 * \{ */

void UI_block_interaction_set(uiBlock *block, uiBlockInteraction_CallbackData *callbacks)
{
  block->custom_interaction_callbacks = *callbacks;
}

static uiBlockInteraction_Handle *ui_block_interaction_begin(bContext *C,
                                                             uiBlock *block,
                                                             const bool is_click)
{
  BLI_assert(block->custom_interaction_callbacks.begin_fn != nullptr);
  uiBlockInteraction_Handle *interaction = MEM_callocN<uiBlockInteraction_Handle>(__func__);

  int unique_retval_ids_len = 0;
  for (const std::unique_ptr<uiBut> &but : block->buttons) {
    if (but->active || (but->flag & UI_BUT_DRAG_MULTI)) {
      unique_retval_ids_len++;
    }
  }

  int *unique_retval_ids = MEM_malloc_arrayN<int>(unique_retval_ids_len, __func__);
  unique_retval_ids_len = 0;
  for (const std::unique_ptr<uiBut> &but : block->buttons) {
    if (but->active || (but->flag & UI_BUT_DRAG_MULTI)) {
      unique_retval_ids[unique_retval_ids_len++] = but->retval;
    }
  }

  if (unique_retval_ids_len > 1) {
    qsort(unique_retval_ids, unique_retval_ids_len, sizeof(int), BLI_sortutil_cmp_int);
    unique_retval_ids_len = BLI_array_deduplicate_ordered(unique_retval_ids,
                                                          unique_retval_ids_len);
    unique_retval_ids = static_cast<int *>(
        MEM_reallocN(unique_retval_ids, sizeof(*unique_retval_ids) * unique_retval_ids_len));
  }

  interaction->params.is_click = is_click;
  interaction->params.unique_retval_ids = unique_retval_ids;
  interaction->params.unique_retval_ids_len = unique_retval_ids_len;

  interaction->user_data = block->custom_interaction_callbacks.begin_fn(
      C, &interaction->params, block->custom_interaction_callbacks.arg1);
  return interaction;
}

static void ui_block_interaction_end(bContext *C,
                                     uiBlockInteraction_CallbackData *callbacks,
                                     uiBlockInteraction_Handle *interaction)
{
  BLI_assert(callbacks->end_fn != nullptr);
  callbacks->end_fn(C, &interaction->params, callbacks->arg1, interaction->user_data);
  MEM_freeN(interaction->params.unique_retval_ids);
  MEM_freeN(interaction);
}

static void ui_block_interaction_update(bContext *C,
                                        uiBlockInteraction_CallbackData *callbacks,
                                        uiBlockInteraction_Handle *interaction)
{
  BLI_assert(callbacks->update_fn != nullptr);
  callbacks->update_fn(C, &interaction->params, callbacks->arg1, interaction->user_data);
}

/**
 * \note #ui_block_interaction_begin cannot be called when setting the button state
 * (e.g. #BUTTON_STATE_NUM_EDITING) for the following reasons.
 *
 * - Other buttons may still be activated using #UI_BUT_DRAG_MULTI
 *   which is necessary before gathering all the #uiBut.retval values to initialize
 *   #uiBlockInteraction_Params.unique_retval_ids.
 * - When clicking on a number button it's not known if the event is a click or a drag.
 *
 * Instead, it must be called immediately before the drag action begins.
 */
static void ui_block_interaction_begin_ensure(bContext *C,
                                              uiBlock *block,
                                              uiHandleButtonData *data,
                                              const bool is_click)
{
  if (data->custom_interaction_handle) {
    return;
  }
  if (block->custom_interaction_callbacks.begin_fn == nullptr) {
    return;
  }

  uiBlockInteraction_Handle *interaction = ui_block_interaction_begin(C, block, is_click);
  interaction->user_count = 1;
  data->custom_interaction_handle = interaction;
}

/** \} */
