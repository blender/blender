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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_colorband.h"
#include "BKE_blender_undo.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"
#include "BKE_unit.h"
#include "BKE_paint.h"

#include "IMB_colormanagement.h"

#include "ED_screen.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BLF_api.h"

#include "interface_intern.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

#ifdef WITH_INPUT_IME
#  include "wm_window.h"
#  include "BLT_translation.h"
#  include "BLT_lang.h"
#endif

/* place the mouse at the scaled down location when un-grabbing */
#define USE_CONT_MOUSE_CORRECT
/* support dragging toggle buttons */
#define USE_DRAG_TOGGLE

/* support dragging multiple number buttons at once */
#define USE_DRAG_MULTINUM

/* allow dragging/editing all other selected items at once */
#define USE_ALLSELECT

/* so we can avoid very small mouse-moves from jumping away from keyboard navigation [#34936] */
#define USE_KEYNAV_LIMIT

/* drag popups by their header */
#define USE_DRAG_POPUP

#define UI_MAX_PASSWORD_STR 128

/* proto */
static int ui_do_but_EXIT(bContext *C,
                          uiBut *but,
                          struct uiHandleButtonData *data,
                          const wmEvent *event);
static bool ui_but_find_select_in_enum__cmp(const uiBut *but_a, const uiBut *but_b);
static void ui_textedit_string_set(uiBut *but, struct uiHandleButtonData *data, const char *str);

#ifdef USE_KEYNAV_LIMIT
static void ui_mouse_motion_keynav_init(struct uiKeyNavLock *keynav, const wmEvent *event);
static bool ui_mouse_motion_keynav_test(struct uiKeyNavLock *keynav, const wmEvent *event);
#endif

/* -------------------------------------------------------------------- */
/** \name Structs & Defines
 * \{ */

#define BUTTON_FLASH_DELAY 0.020
#define MENU_SCROLL_INTERVAL 0.1
#define PIE_MENU_INTERVAL 0.01
#define BUTTON_AUTO_OPEN_THRESH 0.2
#define BUTTON_MOUSE_TOWARDS_THRESH 1.0
/* pixels to move the cursor to get out of keyboard navigation */
#define BUTTON_KEYNAV_PX_LIMIT 8

#define MENU_TOWARDS_MARGIN 20      /* margin in pixels */
#define MENU_TOWARDS_WIGGLE_ROOM 64 /* tolerance in pixels */
/* drag-lock distance threshold in pixels */
#define BUTTON_DRAGLOCK_THRESH 3

typedef enum uiButtonActivateType {
  BUTTON_ACTIVATE_OVER,
  BUTTON_ACTIVATE,
  BUTTON_ACTIVATE_APPLY,
  BUTTON_ACTIVATE_TEXT_EDITING,
  BUTTON_ACTIVATE_OPEN,
} uiButtonActivateType;

typedef enum uiHandleButtonState {
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
} uiHandleButtonState;

#ifdef USE_ALLSELECT

/* Unfortunately there's no good way handle more generally:
 * (propagate single clicks on layer buttons to other objects) */
#  define USE_ALLSELECT_LAYER_HACK

typedef struct uiSelectContextElem {
  PointerRNA ptr;
  union {
    bool val_b;
    int val_i;
    float val_f;
  };
} uiSelectContextElem;

typedef struct uiSelectContextStore {
  uiSelectContextElem *elems;
  int elems_len;
  bool do_free;
  bool is_enabled;
  /* When set, simply copy values (don't apply difference).
   * Rules are:
   * - dragging numbers uses delta.
   * - typing in values will assign to all. */
  bool is_copy;
} uiSelectContextStore;

static bool ui_selectcontext_begin(bContext *C,
                                   uiBut *but,
                                   struct uiSelectContextStore *selctx_data);
static void ui_selectcontext_end(uiBut *but, uiSelectContextStore *selctx_data);
static void ui_selectcontext_apply(bContext *C,
                                   uiBut *but,
                                   struct uiSelectContextStore *selctx_data,
                                   const double value,
                                   const double value_orig);

#  define IS_ALLSELECT_EVENT(event) ((event)->alt != 0)

/** just show a tinted color so users know its activated */
#  define UI_BUT_IS_SELECT_CONTEXT UI_BUT_NODE_ACTIVE

#endif /* USE_ALLSELECT */

#ifdef USE_DRAG_MULTINUM

/**
 * how far to drag before we check for gesture direction (in pixels),
 * note: half the height of a button is about right... */
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
typedef struct uiButMultiState {
  double origvalue;
  uiBut *but;

#  ifdef USE_ALLSELECT
  uiSelectContextStore select_others;
#  endif
} uiButMultiState;

typedef struct uiHandleButtonMulti {
  enum {
    /** gesture direction unknown, wait until mouse has moved enough... */
    BUTTON_MULTI_INIT_UNSET = 0,
    /** vertical gesture detected, flag buttons interactively (UI_BUT_DRAG_MULTI) */
    BUTTON_MULTI_INIT_SETUP,
    /** flag buttons finished, apply horizontal motion to active and flagged */
    BUTTON_MULTI_INIT_ENABLE,
    /** vertical gesture _not_ detected, take no further action */
    BUTTON_MULTI_INIT_DISABLE,
  } init;

  bool has_mbuts; /* any buttons flagged UI_BUT_DRAG_MULTI */
  LinkNode *mbuts;
  uiButStore *bs_mbuts;

  bool is_proportional;

  /* In some cases we directly apply the changes to multiple buttons,
   * so we don't want to do it twice. */
  bool skip;

  /* before activating, we need to check gesture direction accumulate signed cursor movement
   * here so we can tell if this is a vertical motion or not. */
  float drag_dir[2];

  /* values copied direct from event->x,y
   * used to detect buttons between the current and initial mouse position */
  int drag_start[2];

  /* store x location once BUTTON_MULTI_INIT_SETUP is set,
   * moving outside this sets BUTTON_MULTI_INIT_ENABLE */
  int drag_lock_x;

} uiHandleButtonMulti;

#endif /* USE_DRAG_MULTINUM */

typedef struct uiHandleButtonData {
  wmWindowManager *wm;
  wmWindow *window;
  ARegion *region;

  bool interactive;

  /* overall state */
  uiHandleButtonState state;
  int retval;
  /* booleans (could be made into flags) */
  bool cancel, escapecancel;
  bool applied, applied_interactive;
  bool changed_cursor;
  wmTimer *flashtimer;

  /* edited value */
  /* use 'ui_textedit_string_set' to assign new strings */
  char *str;
  char *origstr;
  double value, origvalue, startvalue;
  float vec[3], origvec[3];
#if 0 /* UNUSED */
  int togdual, togonly;
#endif
  ColorBand *coba;

  /* tooltip */
  uint tooltip_force : 1;

  /* auto open */
  bool used_mouse;
  wmTimer *autoopentimer;

  /* auto open (hold) */
  wmTimer *hold_action_timer;

  /* text selection/editing */
  /* size of 'str' (including terminator) */
  int maxlen;
  /* Button text selection:
   * extension direction, selextend, inside ui_do_but_TEX */
  enum {
    EXTEND_NONE = 0,
    EXTEND_LEFT = 1,
    EXTEND_RIGHT = 2,
  } selextend;
  float selstartx;
  /* allow to realloc str/editstr and use 'maxlen' to track alloc size (maxlen + 1) */
  bool is_str_dynamic;

  /* number editing / dragging */
  /* coords are Window/uiBlock relative (depends on the button) */
  int draglastx, draglasty;
  int dragstartx, dragstarty;
  int draglastvalue;
  int dragstartvalue;
  bool dragchange, draglock;
  int dragsel;
  float dragf, dragfstart;
  CBData *dragcbd;

#ifdef USE_CONT_MOUSE_CORRECT
  /* when ungrabbing buttons which are #ui_but_is_cursor_warp(),
   * we may want to position them.
   * FLT_MAX signifies do-nothing, use #ui_block_to_window_fl()
   * to get this into a usable space. */
  float ungrab_mval[2];
#endif

  /* menu open (watch UI_screen_free_active_but) */
  uiPopupBlockHandle *menu;
  int menuretval;

  /* search box (watch UI_screen_free_active_but) */
  ARegion *searchbox;
#ifdef USE_KEYNAV_LIMIT
  struct uiKeyNavLock searchbox_keynav_state;
#endif

#ifdef USE_DRAG_MULTINUM
  /* Multi-buttons will be updated in unison with the active button. */
  uiHandleButtonMulti multi_data;
#endif

#ifdef USE_ALLSELECT
  uiSelectContextStore select_others;
#endif

  /* post activate */
  uiButtonActivateType posttype;
  uiBut *postbut;
} uiHandleButtonData;

typedef struct uiAfterFunc {
  struct uiAfterFunc *next, *prev;

  uiButHandleFunc func;
  void *func_arg1;
  void *func_arg2;

  uiButHandleNFunc funcN;
  void *func_argN;

  uiButHandleRenameFunc rename_func;
  void *rename_arg1;
  void *rename_orig;

  uiBlockHandleFunc handle_func;
  void *handle_func_arg;
  int retval;

  uiMenuHandleFunc butm_func;
  void *butm_func_arg;
  int a2;

  wmOperator *popup_op;
  wmOperatorType *optype;
  int opcontext;
  PointerRNA *opptr;

  PointerRNA rnapoin;
  PropertyRNA *rnaprop;

  bContextStore *context;

  char undostr[BKE_UNDO_STR_MAX];
} uiAfterFunc;

static void button_activate_init(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type);
static void button_activate_state(bContext *C, uiBut *but, uiHandleButtonState state);
static void button_activate_exit(
    bContext *C, uiBut *but, uiHandleButtonData *data, const bool mousemove, const bool onfree);
static int ui_handler_region_menu(bContext *C, const wmEvent *event, void *userdata);
static void ui_handle_button_activate(bContext *C,
                                      ARegion *ar,
                                      uiBut *but,
                                      uiButtonActivateType type);

#ifdef USE_DRAG_MULTINUM
static void ui_multibut_restore(bContext *C, uiHandleButtonData *data, uiBlock *block);
static uiButMultiState *ui_multibut_lookup(uiHandleButtonData *data, const uiBut *but);
#endif

/* buttons clipboard */
static ColorBand but_copypaste_coba = {0};
static CurveMapping but_copypaste_curve = {0};
static bool but_copypaste_curve_alive = false;

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Queries
 * \{ */

bool ui_but_is_editing(const uiBut *but)
{
  uiHandleButtonData *data = but->active;
  return (data && ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING));
}

/* assumes event type is MOUSEPAN */
void ui_pan_to_scroll(const wmEvent *event, int *type, int *val)
{
  static int lastdy = 0;
  int dy = event->prevy - event->y;

  /* This event should be originally from event->type,
   * converting wrong event into wheel is bad, see [#33803] */
  BLI_assert(*type == MOUSEPAN);

  /* sign differs, reset */
  if ((dy > 0 && lastdy < 0) || (dy < 0 && lastdy > 0)) {
    lastdy = dy;
  }
  else {
    lastdy += dy;

    if (ABS(lastdy) > (int)UI_UNIT_Y) {
      if (U.uiflag2 & USER_TRACKPAD_NATURAL) {
        dy = -dy;
      }

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

/**
 * Finds the pressed button in an aligned row (typically an expanded enum).
 *
 * \param direction: Use when there may be multiple buttons pressed.
 */
uiBut *ui_but_find_select_in_enum(uiBut *but, int direction)
{
  uiBut *but_iter = but;
  uiBut *but_found = NULL;
  BLI_assert(ELEM(direction, -1, 1));

  while ((but_iter->prev) && ui_but_find_select_in_enum__cmp(but_iter->prev, but)) {
    but_iter = but_iter->prev;
  }

  while (but_iter && ui_but_find_select_in_enum__cmp(but_iter, but)) {
    if (but_iter->flag & UI_SELECT) {
      but_found = but_iter;
      if (direction == 1) {
        break;
      }
    }
    but_iter = but_iter->next;
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

/**
 * Ignore mouse movements within some horizontal pixel threshold before starting to drag
 */
static bool ui_but_dragedit_update_mval(uiHandleButtonData *data, int mx)
{
  if (mx == data->draglastx) {
    return false;
  }

  if (data->draglock) {
    if (ABS(mx - data->dragstartx) <= BUTTON_DRAGLOCK_THRESH) {
      return false;
    }
#ifdef USE_DRAG_MULTINUM
    if (ELEM(data->multi_data.init, BUTTON_MULTI_INIT_UNSET, BUTTON_MULTI_INIT_SETUP)) {
      return false;
    }
#endif
    data->draglock = false;
    data->dragstartx = mx; /* ignore mouse movement within drag-lock */
  }

  return true;
}

static void ui_but_update_preferences_dirty(uiBut *but)
{
  /* Not very elegant, but ensures preference changes force re-save. */
  bool tag = false;
  if (but->rnaprop) {
    StructRNA *base = RNA_struct_base(but->rnapoin.type);
    if (ELEM(base, &RNA_AddonPreferences, &RNA_KeyConfigPreferences, &RNA_KeyMapItem)) {
      tag = true;
    }
  }

  if (tag) {
    U.runtime.is_dirty = true;
    WM_main_add_notifier(NC_WINDOW, NULL);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Snap Values
 *
 * \{ */

enum eSnapType {
  SNAP_OFF = 0,
  SNAP_ON,
  SNAP_ON_SMALL,
};

static enum eSnapType ui_event_to_snap(const wmEvent *event)
{
  return (event->ctrl) ? (event->shift) ? SNAP_ON_SMALL : SNAP_ON : SNAP_OFF;
}

static bool ui_event_is_snap(const wmEvent *event)
{
  return (ELEM(event->type, LEFTCTRLKEY, RIGHTCTRLKEY) ||
          ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY));
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

static ListBase UIAfterFuncs = {NULL, NULL};

static uiAfterFunc *ui_afterfunc_new(void)
{
  uiAfterFunc *after;

  after = MEM_callocN(sizeof(uiAfterFunc), "uiAfterFunc");

  BLI_addtail(&UIAfterFuncs, after);

  return after;
}

/**
 * For executing operators after the button is pressed.
 * (some non operator buttons need to trigger operators), see: [#37795]
 *
 * \note Can only call while handling buttons.
 */
PointerRNA *ui_handle_afterfunc_add_operator(wmOperatorType *ot, int opcontext, bool create_props)
{
  PointerRNA *ptr = NULL;
  uiAfterFunc *after = ui_afterfunc_new();

  after->optype = ot;
  after->opcontext = opcontext;

  if (create_props) {
    ptr = MEM_callocN(sizeof(PointerRNA), __func__);
    WM_operator_properties_create_ptr(ptr, ot);
    after->opptr = ptr;
  }

  return ptr;
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
  return (but->func || but->funcN || but->rename_func || but->optype || but->rnaprop ||
          block->handle_func || (but->type == UI_BTYPE_BUT_MENU && block->butm_func) ||
          (block->handle && block->handle->popup_op));
}

static void ui_apply_but_func(bContext *C, uiBut *but)
{
  uiAfterFunc *after;
  uiBlock *block = but->block;

  /* these functions are postponed and only executed after all other
   * handling is done, i.e. menus are closed, in order to avoid conflicts
   * with these functions removing the buttons we are working with */

  if (ui_afterfunc_check(block, but)) {
    after = ui_afterfunc_new();

    if (but->func && ELEM(but, but->func_arg1, but->func_arg2)) {
      /* exception, this will crash due to removed button otherwise */
      but->func(C, but->func_arg1, but->func_arg2);
    }
    else {
      after->func = but->func;
    }

    after->func_arg1 = but->func_arg1;
    after->func_arg2 = but->func_arg2;

    after->funcN = but->funcN;
    after->func_argN = (but->func_argN) ? MEM_dupallocN(but->func_argN) : NULL;

    after->rename_func = but->rename_func;
    after->rename_arg1 = but->rename_arg1;
    after->rename_orig = but->rename_orig; /* needs free! */

    after->handle_func = block->handle_func;
    after->handle_func_arg = block->handle_func_arg;
    after->retval = but->retval;

    if (but->type == UI_BTYPE_BUT_MENU) {
      after->butm_func = block->butm_func;
      after->butm_func_arg = block->butm_func_arg;
      after->a2 = but->a2;
    }

    if (block->handle) {
      after->popup_op = block->handle->popup_op;
    }

    after->optype = but->optype;
    after->opcontext = but->opcontext;
    after->opptr = but->opptr;

    after->rnapoin = but->rnapoin;
    after->rnaprop = but->rnaprop;

    if (but->context) {
      after->context = CTX_store_copy(but->context);
    }

    but->optype = NULL;
    but->opcontext = 0;
    but->opptr = NULL;
  }
}

/* typically call ui_apply_but_undo(), ui_apply_but_autokey() */
static void ui_apply_but_undo(uiBut *but)
{
  uiAfterFunc *after;

  if (but->flag & UI_BUT_UNDO) {
    const char *str = NULL;

    /* define which string to use for undo */
    if (but->type == UI_BTYPE_MENU) {
      str = but->drawstr;
    }
    else if (but->drawstr[0]) {
      str = but->drawstr;
    }
    else {
      str = but->tip;
    }

    /* fallback, else we don't get an undo! */
    if (str == NULL || str[0] == '\0') {
      str = "Unknown Action";
    }

    /* delayed, after all other funcs run, popups are closed, etc */
    after = ui_afterfunc_new();
    BLI_strncpy(after->undostr, str, sizeof(after->undostr));
  }
}

static void ui_apply_but_autokey(bContext *C, uiBut *but)
{
  Scene *scene = CTX_data_scene(C);

  /* try autokey */
  ui_but_anim_autokey(C, but, scene, scene->r.cfra);

  /* make a little report about what we've done! */
  if (but->rnaprop) {
    char *buf;

    if (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD) {
      return;
    }

    buf = WM_prop_pystring_assign(C, &but->rnapoin, but->rnaprop, but->rnaindex);
    if (buf) {
      BKE_report(CTX_wm_reports(C), RPT_PROPERTY, buf);
      MEM_freeN(buf);

      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
    }
  }
}

static void ui_apply_but_funcs_after(bContext *C)
{
  uiAfterFunc *afterf, after;
  PointerRNA opptr;
  ListBase funcs;

  /* copy to avoid recursive calls */
  funcs = UIAfterFuncs;
  BLI_listbase_clear(&UIAfterFuncs);

  for (afterf = funcs.first; afterf; afterf = after.next) {
    after = *afterf; /* copy to avoid memleak on exit() */
    BLI_freelinkN(&funcs, afterf);

    if (after.context) {
      CTX_store_set(C, after.context);
    }

    if (after.popup_op) {
      popup_check(C, after.popup_op);
    }

    if (after.opptr) {
      /* free in advance to avoid leak on exit */
      opptr = *after.opptr;
      MEM_freeN(after.opptr);
    }

    if (after.optype) {
      WM_operator_name_call_ptr(C, after.optype, after.opcontext, (after.opptr) ? &opptr : NULL);
    }

    if (after.opptr) {
      WM_operator_properties_free(&opptr);
    }

    if (after.rnapoin.data) {
      RNA_property_update(C, &after.rnapoin, after.rnaprop);
    }

    if (after.context) {
      CTX_store_set(C, NULL);
      CTX_store_free(after.context);
    }

    if (after.func) {
      after.func(C, after.func_arg1, after.func_arg2);
    }
    if (after.funcN) {
      after.funcN(C, after.func_argN, after.func_arg2);
    }
    if (after.func_argN) {
      MEM_freeN(after.func_argN);
    }

    if (after.handle_func) {
      after.handle_func(C, after.handle_func_arg, after.retval);
    }
    if (after.butm_func) {
      after.butm_func(C, after.butm_func_arg, after.a2);
    }

    if (after.rename_func) {
      after.rename_func(C, after.rename_arg1, after.rename_orig);
    }
    if (after.rename_orig) {
      MEM_freeN(after.rename_orig);
    }

    if (after.undostr[0]) {
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
  if (but->type == UI_BTYPE_MENU) {
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
    value_toggle = UI_BITBUT_VALUE_TOGGLED((int)value, but->bitnr);
  }
  else {
    value_toggle = (value == 0.0);
    if (ELEM(but->type, UI_BTYPE_TOGGLE_N, UI_BTYPE_ICON_TOGGLE_N, UI_BTYPE_CHECKBOX_N)) {
      value_toggle = !value_toggle;
    }
  }

  ui_but_value_set(but, (double)value_toggle);
  if (but->type == UI_BTYPE_ICON_TOGGLE || but->type == UI_BTYPE_ICON_TOGGLE_N) {
    ui_but_update_edited(but);
  }

  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_ROW(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
  uiBut *bt;

  ui_but_value_set(but, but->hardmax);

  ui_apply_but_func(C, but);

  /* states of other row buttons */
  for (bt = block->buttons.first; bt; bt = bt->next) {
    if (bt != but && bt->poin == but->poin && ELEM(bt->type, UI_BTYPE_ROW, UI_BTYPE_LISTROW)) {
      ui_but_update_edited(bt);
    }
  }

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_TEX(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (!data->str) {
    return;
  }

  ui_but_string_set(C, but, data->str);
  ui_but_update_edited(but);

  /* give butfunc a copy of the original text too.
   * feature used for bone renaming, channels, etc.
   * afterfunc frees rename_orig */
  if (data->origstr && (but->flag & UI_BUT_TEXTEDIT_UPDATE)) {
    /* In this case, we need to keep origstr available,
     * to restore real org string in case we cancel after having typed something already. */
    but->rename_orig = BLI_strdup(data->origstr);
  }
  /* only if there are afterfuncs, otherwise 'renam_orig' isn't freed */
  else if (ui_afterfunc_check(but->block, but)) {
    but->rename_orig = data->origstr;
    data->origstr = NULL;
  }
  ui_apply_but_func(C, but);

  data->retval = but->retval;
  data->applied = true;
}

static void ui_apply_but_TAB(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (data->str) {
    ui_but_string_set(C, but, data->str);
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
  if (data->str) {
    if (ui_but_string_set(C, but, data->str)) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Drag Multi-Number
 * \{ */

#ifdef USE_DRAG_MULTINUM

/* small multi-but api */
static void ui_multibut_add(uiHandleButtonData *data, uiBut *but)
{
  uiButMultiState *mbut_state;

  BLI_assert(but->flag & UI_BUT_DRAG_MULTI);
  BLI_assert(data->multi_data.has_mbuts);

  mbut_state = MEM_callocN(sizeof(*mbut_state), __func__);
  mbut_state->but = but;
  mbut_state->origvalue = ui_but_value_get(but);

  BLI_linklist_prepend(&data->multi_data.mbuts, mbut_state);

  UI_butstore_register(data->multi_data.bs_mbuts, &mbut_state->but);
}

static uiButMultiState *ui_multibut_lookup(uiHandleButtonData *data, const uiBut *but)
{
  LinkNode *l;

  for (l = data->multi_data.mbuts; l; l = l->next) {
    uiButMultiState *mbut_state;

    mbut_state = l->link;

    if (mbut_state->but == but) {
      return mbut_state;
    }
  }

  return NULL;
}

static void ui_multibut_restore(bContext *C, uiHandleButtonData *data, uiBlock *block)
{
  uiBut *but;

  for (but = block->buttons.first; but; but = but->next) {
    if (but->flag & UI_BUT_DRAG_MULTI) {
      uiButMultiState *mbut_state = ui_multibut_lookup(data, but);
      if (mbut_state) {
        ui_but_value_set(but, mbut_state->origvalue);

#  ifdef USE_ALLSELECT
        if (mbut_state->select_others.elems_len > 0) {
          ui_selectcontext_apply(
              C, but, &mbut_state->select_others, mbut_state->origvalue, mbut_state->origvalue);
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
      uiButMultiState *mbut_state = list->link;

      if (mbut_state->select_others.elems) {
        MEM_freeN(mbut_state->select_others.elems);
      }

      MEM_freeN(list->link);
      MEM_freeN(list);
      list = next;
    }
  }
#  else
  BLI_linklist_freeN(data->multi_data.mbuts);
#  endif

  data->multi_data.mbuts = NULL;

  if (data->multi_data.bs_mbuts) {
    UI_butstore_free(block, data->multi_data.bs_mbuts);
    data->multi_data.bs_mbuts = NULL;
  }
}

static bool ui_multibut_states_tag(uiBut *but_active,
                                   uiHandleButtonData *data,
                                   const wmEvent *event)
{
  uiBut *but;
  float seg[2][2];
  bool changed = false;

  seg[0][0] = data->multi_data.drag_start[0];
  seg[0][1] = data->multi_data.drag_start[1];

  seg[1][0] = event->x;
  seg[1][1] = event->y;

  BLI_assert(data->multi_data.init == BUTTON_MULTI_INIT_SETUP);

  ui_window_to_block_fl(data->region, but_active->block, &seg[0][0], &seg[0][1]);
  ui_window_to_block_fl(data->region, but_active->block, &seg[1][0], &seg[1][1]);

  data->multi_data.has_mbuts = false;

  /* follow ui_but_find_mouse_over_ex logic */
  for (but = but_active->block->buttons.first; but; but = but->next) {
    bool drag_prev = false;
    bool drag_curr = false;

    /* re-set each time */
    if (but->flag & UI_BUT_DRAG_MULTI) {
      but->flag &= ~UI_BUT_DRAG_MULTI;
      drag_prev = true;
    }

    if (ui_but_is_interactive(but, false)) {

      /* drag checks */
      if (but_active != but) {
        if (ui_but_is_compatible(but_active, but)) {

          BLI_assert(but->active == NULL);

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
  BLI_assert(data->multi_data.init == BUTTON_MULTI_INIT_SETUP);
  BLI_assert(data->multi_data.has_mbuts);

  data->multi_data.bs_mbuts = UI_butstore_create(but_active->block);

  for (uiBut *but = but_active->block->buttons.first; but; but = but->next) {
    if (but->flag & UI_BUT_DRAG_MULTI) {
      ui_multibut_add(data, but);
    }
  }

  /* edit buttons proportionally to eachother
   * note: if we mix buttons which are proportional and others which are not,
   * this may work a bit strangely */
  if ((but_active->rnaprop && (RNA_property_flag(but_active->rnaprop) & PROP_PROPORTIONAL)) ||
      ELEM(but_active->unit_type, PROP_UNIT_LENGTH)) {
    if (data->origvalue != 0.0) {
      data->multi_data.is_proportional = true;
    }
  }
}

static void ui_multibut_states_apply(bContext *C, uiHandleButtonData *data, uiBlock *block)
{
  ARegion *ar = data->region;
  const double value_delta = data->value - data->origvalue;
  const double value_scale = data->multi_data.is_proportional ? (data->value / data->origvalue) :
                                                                0.0;
  uiBut *but;

  BLI_assert(data->multi_data.init == BUTTON_MULTI_INIT_ENABLE);
  BLI_assert(data->multi_data.skip == false);

  for (but = block->buttons.first; but; but = but->next) {
    if (but->flag & UI_BUT_DRAG_MULTI) {
      /* mbut_states for delta */
      uiButMultiState *mbut_state = ui_multibut_lookup(data, but);

      if (mbut_state) {
        void *active_back;

        ui_but_execute_begin(C, ar, but, &active_back);

#  ifdef USE_ALLSELECT
        if (data->select_others.is_enabled) {
          /* init once! */
          if (mbut_state->select_others.elems_len == 0) {
            ui_selectcontext_begin(C, but, &mbut_state->select_others);
          }
          if (mbut_state->select_others.elems_len == 0) {
            mbut_state->select_others.elems_len = -1;
          }
        }

        /* needed so we apply the right deltas */
        but->active->origvalue = mbut_state->origvalue;
        but->active->select_others = mbut_state->select_others;
        but->active->select_others.do_free = false;
#  endif

        BLI_assert(active_back == NULL);
        /* no need to check 'data->state' here */
        if (data->str) {
          /* entering text (set all) */
          but->active->value = data->value;
          ui_but_string_set(C, but, data->str);
        }
        else {
          /* dragging (use delta) */
          if (data->multi_data.is_proportional) {
            but->active->value = mbut_state->origvalue * value_scale;
          }
          else {
            but->active->value = mbut_state->origvalue + value_delta;
          }

          /* clamp based on soft limits, see: T40154 */
          CLAMP(but->active->value, (double)but->softmin, (double)but->softmax);
        }
        ui_but_execute_end(C, ar, but, active_back);
      }
      else {
        /* highly unlikely */
        printf("%s: cant find button\n", __func__);
      }
      /* end */
    }
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
  if (ui_but_is_bool(but)) {
    return true;
  }
  else if (UI_but_is_decorator(but)) {
    return ELEM(but->icon,
                ICON_DECORATE,
                ICON_DECORATE_KEYFRAME,
                ICON_DECORATE_ANIMATE,
                ICON_DECORATE_OVERRIDE);
  }
  else {
    return false;
  }
}

/* Button pushed state to compare if other buttons match. Can be more
 * then just true or false for toggle buttons with more than 2 states. */
static int ui_drag_toggle_but_pushed_state(bContext *C, uiBut *but)
{
  if (but->rnapoin.data == NULL && but->poin == NULL && but->icon) {
    if (but->pushed_state_func) {
      return but->pushed_state_func(C, but->pushed_state_arg);
    }
    else {
      /* Assume icon identifies a unique state, for buttons that
       * work though functions callbacks and don't have an boolean
       * value that indicates the state. */
      return but->icon + but->iconadd;
    }
  }
  else if (ui_but_is_bool(but)) {
    return ui_but_is_pushed(but);
  }
  else {
    return 0;
  }
}

typedef struct uiDragToggleHandle {
  /* init */
  int pushed_state;
  float but_cent_start[2];

  bool is_xy_lock_init;
  bool xy_lock[2];

  int xy_init[2];
  int xy_last[2];
} uiDragToggleHandle;

static bool ui_drag_toggle_set_xy_xy(
    bContext *C, ARegion *ar, const int pushed_state, const int xy_src[2], const int xy_dst[2])
{
  /* popups such as layers won't re-evaluate on redraw */
  const bool do_check = (ar->regiontype == RGN_TYPE_TEMPORARY);
  bool changed = false;
  uiBlock *block;

  for (block = ar->uiblocks.first; block; block = block->next) {
    uiBut *but;

    float xy_a_block[2] = {UNPACK2(xy_src)};
    float xy_b_block[2] = {UNPACK2(xy_dst)};

    ui_window_to_block_fl(ar, block, &xy_a_block[0], &xy_a_block[1]);
    ui_window_to_block_fl(ar, block, &xy_b_block[0], &xy_b_block[1]);

    for (but = block->buttons.first; but; but = but->next) {
      /* Note: ctrl is always true here because (at least for now)
       * we always want to consider text control in this case, even when not embossed. */
      if (ui_but_is_interactive(but, true)) {
        if (BLI_rctf_isect_segment(&but->rect, xy_a_block, xy_b_block)) {

          /* execute the button */
          if (ui_drag_toggle_but_is_supported(but)) {
            /* is it pressed? */
            int pushed_state_but = ui_drag_toggle_but_pushed_state(C, but);
            if (pushed_state_but != pushed_state) {
              UI_but_execute(C, but);
              if (do_check) {
                ui_but_update_edited(but);
              }
              if (U.runtime.is_dirty == false) {
                ui_but_update_preferences_dirty(but);
              }
              changed = true;
            }
          }
          /* done */
        }
      }
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
  ARegion *ar = CTX_wm_region(C);
  bool do_draw = false;
  int xy[2];

  /**
   * Initialize Locking:
   *
   * Check if we need to initialize the lock axis by finding if the first
   * button we mouse over is X or Y aligned, then lock the mouse to that axis after.
   */
  if (drag_info->is_xy_lock_init == false) {
    /* first store the buttons original coords */
    uiBut *but = ui_but_find_mouse_over_ex(ar, xy_input[0], xy_input[1], true);

    if (but) {
      if (but->flag & UI_BUT_DRAG_LOCK) {
        const float but_cent_new[2] = {
            BLI_rctf_cent_x(&but->rect),
            BLI_rctf_cent_y(&but->rect),
        };

        /* check if this is a different button,
         * chances are high the button wont move about :) */
        if (len_manhattan_v2v2(drag_info->but_cent_start, but_cent_new) > 1.0f) {
          if (fabsf(drag_info->but_cent_start[0] - but_cent_new[0]) <
              fabsf(drag_info->but_cent_start[1] - but_cent_new[1])) {
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

  xy[0] = (drag_info->xy_lock[0] == false) ? xy_input[0] : drag_info->xy_last[0];
  xy[1] = (drag_info->xy_lock[1] == false) ? xy_input[1] : drag_info->xy_last[1];

  /* touch all buttons between last mouse coord and this one */
  do_draw = ui_drag_toggle_set_xy_xy(C, ar, drag_info->pushed_state, drag_info->xy_last, xy);

  if (do_draw) {
    ED_region_tag_redraw(ar);
  }

  copy_v2_v2_int(drag_info->xy_last, xy);
}

static void ui_handler_region_drag_toggle_remove(bContext *UNUSED(C), void *userdata)
{
  uiDragToggleHandle *drag_info = userdata;
  MEM_freeN(drag_info);
}

static int ui_handler_region_drag_toggle(bContext *C, const wmEvent *event, void *userdata)
{
  uiDragToggleHandle *drag_info = userdata;
  bool done = false;

  switch (event->type) {
    case LEFTMOUSE: {
      if (event->val == KM_RELEASE) {
        done = true;
      }
      break;
    }
    case MOUSEMOVE: {
      ui_drag_toggle_set(C, drag_info, &event->x);
      break;
    }
  }

  if (done) {
    wmWindow *win = CTX_wm_window(C);
    ARegion *ar = CTX_wm_region(C);
    uiBut *but = ui_but_find_mouse_over_ex(ar, drag_info->xy_init[0], drag_info->xy_init[1], true);

    if (but) {
      ui_apply_but_undo(but);
    }

    WM_event_remove_ui_handler(&win->modalhandlers,
                               ui_handler_region_drag_toggle,
                               ui_handler_region_drag_toggle_remove,
                               drag_info,
                               false);
    ui_handler_region_drag_toggle_remove(C, drag_info);

    WM_event_add_mousemove(C);
    return WM_UI_HANDLER_BREAK;
  }
  else {
    return WM_UI_HANDLER_CONTINUE;
  }
}

static bool ui_but_is_drag_toggle(const uiBut *but)
{
  return ((ui_drag_toggle_but_is_supported(but) == true) &&
          /* menu check is importnt so the button dragged over isn't removed instantly */
          (ui_block_is_menu(but->block) == false));
}

#endif /* USE_DRAG_TOGGLE */

#ifdef USE_ALLSELECT

static bool ui_selectcontext_begin(bContext *C, uiBut *but, uiSelectContextStore *selctx_data)
{
  PointerRNA ptr, lptr, idptr;
  PropertyRNA *prop, *lprop;
  bool success = false;
  int index;

  char *path = NULL;
  ListBase lb = {NULL};

  ptr = but->rnapoin;
  prop = but->rnaprop;
  index = but->rnaindex;

  /* for now don't support whole colors */
  if (index == -1) {
    return false;
  }

  /* if there is a valid property that is editable... */
  if (ptr.data && prop) {
    CollectionPointerLink *link;
    bool use_path_from_id;
    int i;

    /* some facts we want to know */
    const bool is_array = RNA_property_array_check(prop);
    const int rna_type = RNA_property_type(prop);

    if (UI_context_copy_to_selected_list(C, &ptr, prop, &lb, &use_path_from_id, &path) &&
        !BLI_listbase_is_empty(&lb)) {
      selctx_data->elems_len = BLI_listbase_count(&lb);
      selctx_data->elems = MEM_mallocN(sizeof(uiSelectContextElem) * selctx_data->elems_len,
                                       __func__);

      for (i = 0, link = lb.first; i < selctx_data->elems_len; i++, link = link->next) {
        uiSelectContextElem *other = &selctx_data->elems[i];
        /* TODO,. de-duplicate copy_to_selected_button */
        if (link->ptr.data != ptr.data) {
          if (use_path_from_id) {
            /* Path relative to ID. */
            lprop = NULL;
            RNA_id_pointer_create(link->ptr.id.data, &idptr);
            RNA_path_resolve_property(&idptr, path, &lptr, &lprop);
          }
          else if (path) {
            /* Path relative to elements from list. */
            lprop = NULL;
            RNA_path_resolve_property(&link->ptr, path, &lptr, &lprop);
          }
          else {
            lptr = link->ptr;
            lprop = prop;
          }

          /* lptr might not be the same as link->ptr! */
          if ((lptr.data != ptr.data) && (lprop == prop) && RNA_property_editable(&lptr, lprop)) {
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

            continue;
          }
        }

        selctx_data->elems_len -= 1;
        i -= 1;
      }

      success = (selctx_data->elems_len != 0);
    }
  }

  if (selctx_data->elems_len == 0) {
    MEM_SAFE_FREE(selctx_data->elems);
  }

  MEM_SAFE_FREE(path);
  BLI_freelistN(&lb);

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
    if (selctx_data->elems) {
      MEM_freeN(selctx_data->elems);
    }
  }

  but->flag &= ~UI_BUT_IS_SELECT_CONTEXT;
}

static void ui_selectcontext_apply(bContext *C,
                                   uiBut *but,
                                   uiSelectContextStore *selctx_data,
                                   const double value,
                                   const double value_orig)
{
  if (selctx_data->elems) {
    PropertyRNA *prop = but->rnaprop;
    PropertyRNA *lprop = but->rnaprop;
    int index = but->rnaindex;
    int i;
    const bool use_delta = (selctx_data->is_copy == false);

    union {
      bool b;
      int i;
      float f;
      PointerRNA p;
    } delta, min, max;

    const bool is_array = RNA_property_array_check(prop);
    const int rna_type = RNA_property_type(prop);

    if (rna_type == PROP_FLOAT) {
      delta.f = use_delta ? (value - value_orig) : value;
      RNA_property_float_range(&but->rnapoin, prop, &min.f, &max.f);
    }
    else if (rna_type == PROP_INT) {
      delta.i = use_delta ? ((int)value - (int)value_orig) : (int)value;
      RNA_property_int_range(&but->rnapoin, prop, &min.i, &max.i);
    }
    else if (rna_type == PROP_ENUM) {
      /* not a delta infact */
      delta.i = RNA_property_enum_get(&but->rnapoin, prop);
    }
    else if (rna_type == PROP_BOOLEAN) {
      if (is_array) {
        /* not a delta infact */
        delta.b = RNA_property_boolean_get_index(&but->rnapoin, prop, index);
      }
      else {
        /* not a delta infact */
        delta.b = RNA_property_boolean_get(&but->rnapoin, prop);
      }
    }
    else if (rna_type == PROP_POINTER) {
      /* not a delta infact */
      delta.p = RNA_property_pointer_get(&but->rnapoin, prop);
    }

#  ifdef USE_ALLSELECT_LAYER_HACK
    /* make up for not having 'handle_layer_buttons' */
    {
      PropertySubType subtype = RNA_property_subtype(prop);

      if ((rna_type == PROP_BOOLEAN) && ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER) && is_array &&
          /* could check for 'handle_layer_buttons' */
          but->func) {
        wmWindow *win = CTX_wm_window(C);
        if (!win->eventstate->shift) {
          const int len = RNA_property_array_length(&but->rnapoin, prop);
          bool *tmparray = MEM_callocN(sizeof(bool) * len, __func__);

          tmparray[index] = true;

          for (i = 0; i < selctx_data->elems_len; i++) {
            uiSelectContextElem *other = &selctx_data->elems[i];
            PointerRNA lptr = other->ptr;
            RNA_property_boolean_set_array(&lptr, lprop, tmparray);
            RNA_property_update(C, &lptr, lprop);
          }

          MEM_freeN(tmparray);

          return;
        }
      }
    }
#  endif

    for (i = 0; i < selctx_data->elems_len; i++) {
      uiSelectContextElem *other = &selctx_data->elems[i];
      PointerRNA lptr = other->ptr;

      if (rna_type == PROP_FLOAT) {
        float other_value = use_delta ? (other->val_f + delta.f) : delta.f;
        CLAMP(other_value, min.f, max.f);
        if (is_array) {
          RNA_property_float_set_index(&lptr, lprop, index, other_value);
        }
        else {
          RNA_property_float_set(&lptr, lprop, other_value);
        }
      }
      else if (rna_type == PROP_INT) {
        int other_value = use_delta ? (other->val_i + delta.i) : delta.i;
        CLAMP(other_value, min.i, max.i);
        if (is_array) {
          RNA_property_int_set_index(&lptr, lprop, index, other_value);
        }
        else {
          RNA_property_int_set(&lptr, lprop, other_value);
        }
      }
      else if (rna_type == PROP_BOOLEAN) {
        const bool other_value = delta.b;
        if (is_array) {
          RNA_property_boolean_set_index(&lptr, lprop, index, other_value);
        }
        else {
          RNA_property_boolean_set(&lptr, lprop, delta.b);
        }
      }
      else if (rna_type == PROP_ENUM) {
        const int other_value = delta.i;
        BLI_assert(!is_array);
        RNA_property_enum_set(&lptr, lprop, other_value);
      }
      else if (rna_type == PROP_POINTER) {
        const PointerRNA other_value = delta.p;
        RNA_property_pointer_set(&lptr, lprop, other_value, NULL);
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
  WM_gestures_remove(C);

  /* Clamp the maximum to half the UI unit size so a high user preference
   * doesn't require the user to drag more then half the default button height. */
  const int drag_threshold = min_ii(
      WM_EVENT_CURSOR_CLICK_DRAG_THRESHOLD,
      (int)((UI_UNIT_Y / 2) * ui_block_to_window_scale(data->region, but->block)));

  if (ABS(data->dragstartx - event->x) + ABS(data->dragstarty - event->y) > drag_threshold) {
    button_activate_state(C, but, BUTTON_STATE_EXIT);
    data->cancel = true;
#ifdef USE_DRAG_TOGGLE
    if (ui_drag_toggle_but_is_supported(but)) {
      uiDragToggleHandle *drag_info = MEM_callocN(sizeof(*drag_info), __func__);
      ARegion *ar_prev;

      /* call here because regular mouse-up event wont run,
       * typically 'button_activate_exit()' handles this */
      ui_apply_but_autokey(C, but);

      drag_info->pushed_state = ui_drag_toggle_but_pushed_state(C, but);
      drag_info->but_cent_start[0] = BLI_rctf_cent_x(&but->rect);
      drag_info->but_cent_start[1] = BLI_rctf_cent_y(&but->rect);
      copy_v2_v2_int(drag_info->xy_init, &event->x);
      copy_v2_v2_int(drag_info->xy_last, &event->x);

      /* needed for toggle drag on popups */
      ar_prev = CTX_wm_region(C);
      CTX_wm_region_set(C, data->region);

      WM_event_add_ui_handler(C,
                              &data->window->modalhandlers,
                              ui_handler_region_drag_toggle,
                              ui_handler_region_drag_toggle_remove,
                              drag_info,
                              WM_HANDLER_BLOCKING);

      CTX_wm_region_set(C, ar_prev);

      /* Initialize alignment for single row/column regions,
       * otherwise we use the relative position of the first other button dragged over. */
      if (ELEM(data->region->regiontype,
               RGN_TYPE_NAV_BAR,
               RGN_TYPE_HEADER,
               RGN_TYPE_TOOL_HEADER,
               RGN_TYPE_FOOTER)) {
        int lock_axis = -1;
        if (ELEM(data->region->alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
          lock_axis = 0;
        }
        else if (ELEM(data->region->alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
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
        if (but->type == UI_BTYPE_COLOR) {
      bool valid = false;
      uiDragColorHandle *drag_info = MEM_callocN(sizeof(*drag_info), __func__);

      /* TODO support more button pointer types */
      if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
        ui_but_v3_get(but, drag_info->color);
        drag_info->gamma_corrected = true;
        valid = true;
      }
      else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
        ui_but_v3_get(but, drag_info->color);
        drag_info->gamma_corrected = false;
        valid = true;
      }
      else if (ELEM(but->pointype, UI_BUT_POIN_FLOAT, UI_BUT_POIN_CHAR)) {
        ui_but_v3_get(but, drag_info->color);
        copy_v3_v3(drag_info->color, (float *)but->poin);
        valid = true;
      }

      if (valid) {
        WM_event_start_drag(C, ICON_COLOR, WM_DRAG_COLOR, drag_info, 0.0, WM_DRAG_FREE_DATA);
      }
      else {
        MEM_freeN(drag_info);
        return false;
      }
    }
    else {
      wmDrag *drag = WM_event_start_drag(
          C, but->icon, but->dragtype, but->dragpoin, ui_but_value_get(but), WM_DRAG_NOP);

      if (but->imb) {
        WM_event_drag_image(drag,
                            but->imb,
                            but->imb_scale,
                            BLI_rctf_size_x(&but->rect),
                            BLI_rctf_size_y(&but->rect));
      }
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
  char *editstr;
  double *editval;
  float *editvec;
  ColorBand *editcoba;
  CurveMapping *editcumap;

  data->retval = 0;

  /* if we cancel and have not applied yet, there is nothing to do,
   * otherwise we have to restore the original value again */
  if (data->cancel) {
    if (!data->applied) {
      return;
    }

    if (data->str) {
      MEM_freeN(data->str);
    }
    data->str = data->origstr;
    data->origstr = NULL;
    data->value = data->origvalue;
    copy_v3_v3(data->vec, data->origvec);
    /* postpone clearing origdata */
  }
  else {
    /* we avoid applying interactive edits a second time
     * at the end with the appliedinteractive flag */
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
        if (data->select_others.elems_len == 0) {
      wmWindow *win = CTX_wm_window(C);
      /* may have been enabled before activating */
      if (data->select_others.is_enabled || IS_ALLSELECT_EVENT(win->eventstate)) {
        ui_selectcontext_begin(C, but, &data->select_others);
        data->select_others.is_enabled = true;
      }
    }
    if (data->select_others.elems_len == 0) {
      /* dont check again */
      data->select_others.elems_len = -1;
    }
#endif
  }

  /* ensures we are writing actual values */
  editstr = but->editstr;
  editval = but->editval;
  editvec = but->editvec;
  editcoba = but->editcoba;
  editcumap = but->editcumap;
  but->editstr = NULL;
  but->editval = NULL;
  but->editvec = NULL;
  but->editcoba = NULL;
  but->editcumap = NULL;

  /* handle different types */
  switch (but->type) {
    case UI_BTYPE_BUT:
      ui_apply_but_BUT(C, but, data);
      break;
    case UI_BTYPE_TEXT:
    case UI_BTYPE_SEARCH_MENU:
      ui_apply_but_TEX(C, but, data);
      break;
    case UI_BTYPE_BUT_TOGGLE:
    case UI_BTYPE_TOGGLE:
    case UI_BTYPE_TOGGLE_N:
    case UI_BTYPE_ICON_TOGGLE:
    case UI_BTYPE_ICON_TOGGLE_N:
    case UI_BTYPE_CHECKBOX:
    case UI_BTYPE_CHECKBOX_N:
      ui_apply_but_TOG(C, but, data);
      break;
    case UI_BTYPE_ROW:
    case UI_BTYPE_LISTROW:
      ui_apply_but_ROW(C, block, but, data);
      break;
    case UI_BTYPE_TAB:
      ui_apply_but_TAB(C, but, data);
      break;
    case UI_BTYPE_SCROLL:
    case UI_BTYPE_GRIP:
    case UI_BTYPE_NUM:
    case UI_BTYPE_NUM_SLIDER:
      ui_apply_but_NUM(C, but, data);
      break;
    case UI_BTYPE_MENU:
    case UI_BTYPE_BLOCK:
    case UI_BTYPE_PULLDOWN:
      ui_apply_but_BLOCK(C, but, data);
      break;
    case UI_BTYPE_COLOR:
      if (data->cancel) {
        ui_apply_but_VEC(C, but, data);
      }
      else {
        ui_apply_but_BLOCK(C, but, data);
      }
      break;
    case UI_BTYPE_BUT_MENU:
      ui_apply_but_BUTM(C, but, data);
      break;
    case UI_BTYPE_UNITVEC:
    case UI_BTYPE_HSVCUBE:
    case UI_BTYPE_HSVCIRCLE:
      ui_apply_but_VEC(C, but, data);
      break;
    case UI_BTYPE_COLORBAND:
      ui_apply_but_COLORBAND(C, but, data);
      break;
    case UI_BTYPE_CURVE:
      ui_apply_but_CURVE(C, but, data);
      break;
    case UI_BTYPE_KEY_EVENT:
    case UI_BTYPE_HOTKEY_EVENT:
      ui_apply_but_BUT(C, but, data);
      break;
    case UI_BTYPE_IMAGE:
      ui_apply_but_IMAGE(C, but, data);
      break;
    case UI_BTYPE_HISTOGRAM:
      ui_apply_but_HISTOGRAM(C, but, data);
      break;
    case UI_BTYPE_WAVEFORM:
      ui_apply_but_WAVEFORM(C, but, data);
      break;
    case UI_BTYPE_TRACK_PREVIEW:
      ui_apply_but_TRACKPREVIEW(C, but, data);
      break;
    default:
      break;
  }

#ifdef USE_DRAG_MULTINUM
  if (data->multi_data.has_mbuts) {
    if ((data->multi_data.init == BUTTON_MULTI_INIT_ENABLE) && (data->multi_data.skip == false)) {
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
  but->editcoba = editcoba;
  but->editcumap = editcumap;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Drop Event
 * \{ */

/* only call if event type is EVT_DROP */
static void ui_but_drop(bContext *C, const wmEvent *event, uiBut *but, uiHandleButtonData *data)
{
  wmDrag *wmd;
  ListBase *drags = event->customdata; /* drop event type has listbase customdata by default */

  for (wmd = drags->first; wmd; wmd = wmd->next) {
    if (wmd->type == WM_DRAG_ID) {
      /* align these types with UI_but_active_drop_name */
      if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
        ID *id = WM_drag_ID(wmd, 0);

        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);

        ui_textedit_string_set(but, data, id->name + 2);

        if (ELEM(but->type, UI_BTYPE_SEARCH_MENU)) {
          but->changed = true;
          ui_searchbox_update(C, data->searchbox, but, true);
        }

        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Copy & Paste
 * \{ */

static void ui_but_get_pasted_text_from_clipboard(char **buf_paste, int *buf_len)
{
  char *text;
  int length;
  /* get only first line even if the clipboard contains multiple lines */
  text = WM_clipboard_text_get_firstline(false, &length);

  if (text) {
    *buf_paste = text;
    *buf_len = length;
  }
  else {
    *buf_paste = MEM_callocN(sizeof(char), __func__);
    *buf_len = 0;
  }
}

static int get_but_property_array_length(uiBut *but)
{
  return RNA_property_array_length(&but->rnapoin, but->rnaprop);
}

static void ui_but_set_float_array(
    bContext *C, uiBut *but, uiHandleButtonData *data, float *values, int array_length)
{
  button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

  for (int i = 0; i < array_length; i++) {
    RNA_property_float_set_index(&but->rnapoin, but->rnaprop, i, values[i]);
  }
  if (data) {
    data->value = values[but->rnaindex];
  }

  button_activate_state(C, but, BUTTON_STATE_EXIT);
}

static void float_array_to_string(float *values,
                                  int array_length,
                                  char *output,
                                  int output_len_max)
{
  /* to avoid buffer overflow attacks; numbers are quite arbitrary */
  BLI_assert(output_len_max > 15);
  output_len_max -= 10;

  int current_index = 0;
  output[current_index] = '[';
  current_index++;

  for (int i = 0; i < array_length; i++) {
    int length = BLI_snprintf(
        output + current_index, output_len_max - current_index, "%f", values[i]);
    current_index += length;

    if (i < array_length - 1) {
      if (current_index < output_len_max) {
        output[current_index + 0] = ',';
        output[current_index + 1] = ' ';
        current_index += 2;
      }
    }
  }

  output[current_index + 0] = ']';
  output[current_index + 1] = '\0';
}

static void ui_but_copy_numeric_array(uiBut *but, char *output, int output_len_max)
{
  int array_length = get_but_property_array_length(but);
  float *values = alloca(array_length * sizeof(float));
  RNA_property_float_get_array(&but->rnapoin, but->rnaprop, values);
  float_array_to_string(values, array_length, output, output_len_max);
}

static bool parse_float_array(char *text, float *values, int expected_length)
{
  /* can parse max 4 floats for now */
  BLI_assert(0 <= expected_length && expected_length <= 4);

  float v[5];
  int actual_length = sscanf(text, "[%f, %f, %f, %f, %f]", &v[0], &v[1], &v[2], &v[3], &v[4]);

  if (actual_length == expected_length) {
    memcpy(values, v, sizeof(float) * expected_length);
    return true;
  }
  else {
    return false;
  }
}

static void ui_but_paste_numeric_array(bContext *C,
                                       uiBut *but,
                                       uiHandleButtonData *data,
                                       char *buf_paste)
{
  int array_length = get_but_property_array_length(but);
  if (array_length > 4) {
    // not supported for now
    return;
  }

  float *values = alloca(sizeof(float) * array_length);

  if (parse_float_array(buf_paste, values, array_length)) {
    ui_but_set_float_array(C, but, data, values, array_length);
  }
  else {
    WM_report(RPT_ERROR, "Expected an array of numbers: [n, n, ...]");
  }
}

static void ui_but_copy_numeric_value(uiBut *but, char *output, int output_len_max)
{
  /* Get many decimal places, then strip trailing zeros.
   * note: too high values start to give strange results */
  ui_but_string_get_ex(but, output, output_len_max, UI_PRECISION_FLOAT_MAX, false, NULL);
  BLI_str_rstrip_float_zero(output, '\0');
}

static void ui_but_paste_numeric_value(bContext *C,
                                       uiBut *but,
                                       uiHandleButtonData *data,
                                       char *buf_paste)
{
  double value;

  if (ui_but_string_set_eval_num(C, but, buf_paste, &value)) {
    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    data->value = value;
    ui_but_string_set(C, but, buf_paste);
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
  else {
    WM_report(RPT_ERROR, "Expected a number");
  }
}

static void ui_but_paste_normalized_vector(bContext *C, uiBut *but, char *buf_paste)
{
  float xyz[3];
  if (parse_float_array(buf_paste, xyz, 3)) {
    if (normalize_v3(xyz) == 0.0f) {
      /* better set Z up then have a zero vector */
      xyz[2] = 1.0;
    }
    ui_but_set_float_array(C, but, NULL, xyz, 3);
  }
  else {
    WM_report(RPT_ERROR, "Paste expected 3 numbers, formatted: '[n, n, n]'");
  }
}

static void ui_but_copy_color(uiBut *but, char *output, int output_len_max)
{
  float rgba[4];

  if (but->rnaprop && get_but_property_array_length(but) == 4) {
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

  float_array_to_string(rgba, 4, output, output_len_max);
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
      int array_len = get_but_property_array_length(but);
      BLI_assert(ELEM(array_len, 3, 4));
      ui_but_set_float_array(C, but, NULL, rgba, array_len);
    }
  }
  else {
    WM_report(RPT_ERROR, "Paste expected 4 numbers, formatted: '[n, n, n, n]'");
  }
}

static void ui_but_copy_text(uiBut *but, char *output, int output_len_max)
{
  ui_but_string_get(but, output, output_len_max);
}

static void ui_but_paste_text(bContext *C, uiBut *but, uiHandleButtonData *data, char *buf_paste)
{
  button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
  ui_textedit_string_set(but, but->active, buf_paste);

  if (but->type == UI_BTYPE_SEARCH_MENU) {
    but->changed = true;
    ui_searchbox_update(C, data->searchbox, but, true);
  }

  button_activate_state(C, but, BUTTON_STATE_EXIT);
}

static void ui_but_copy_colorband(uiBut *but)
{
  if (but->poin != NULL) {
    memcpy(&but_copypaste_coba, but->poin, sizeof(ColorBand));
  }
}

static void ui_but_paste_colorband(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (but_copypaste_coba.tot != 0) {
    if (!but->poin) {
      but->poin = MEM_callocN(sizeof(ColorBand), "colorband");
    }

    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    memcpy(data->coba, &but_copypaste_coba, sizeof(ColorBand));
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
}

static void ui_but_copy_curvemapping(uiBut *but)
{
  if (but->poin != NULL) {
    but_copypaste_curve_alive = true;
    curvemapping_free_data(&but_copypaste_curve);
    curvemapping_copy_data(&but_copypaste_curve, (CurveMapping *)but->poin);
  }
}

static void ui_but_paste_curvemapping(bContext *C, uiBut *but)
{
  if (but_copypaste_curve_alive) {
    if (!but->poin) {
      but->poin = MEM_callocN(sizeof(CurveMapping), "curvemapping");
    }

    button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
    curvemapping_free_data((CurveMapping *)but->poin);
    curvemapping_copy_data((CurveMapping *)but->poin, &but_copypaste_curve);
    button_activate_state(C, but, BUTTON_STATE_EXIT);
  }
}

static void ui_but_copy_operator(bContext *C, uiBut *but, char *output, int output_len_max)
{
  PointerRNA *opptr;
  opptr = UI_but_operator_ptr_get(but);

  char *str;
  str = WM_operator_pystring_ex(C, NULL, false, true, but->optype, opptr);
  BLI_strncpy(output, str, output_len_max);
  MEM_freeN(str);
}

static bool ui_but_copy_menu(uiBut *but, char *output, int output_len_max)
{
  MenuType *mt = UI_but_menutype_get(but);
  if (mt) {
    BLI_snprintf(output, output_len_max, "bpy.ops.wm.call_menu(name=\"%s\")", mt->idname);
    return true;
  }
  return false;
}

static bool ui_but_copy_popover(uiBut *but, char *output, int output_len_max)
{
  PanelType *pt = UI_but_paneltype_get(but);
  if (pt) {
    BLI_snprintf(output, output_len_max, "bpy.ops.wm.call_panel(name=\"%s\")", pt->idname);
    return true;
  }
  return false;
}

static void ui_but_copy(bContext *C, uiBut *but, const bool copy_array)
{
  if (ui_but_contains_password(but)) {
    return;
  }

  /* Arbitrary large value (allow for paths: 'PATH_MAX') */
  char buf[4096] = {0};
  const int buf_max_len = sizeof(buf);

  /* Left false for copying internal data (color-band for eg). */
  bool is_buf_set = false;

  bool has_required_data = !(but->poin == NULL && but->rnapoin.data == NULL);

  switch (but->type) {
    case UI_BTYPE_NUM:
    case UI_BTYPE_NUM_SLIDER:
      if (!has_required_data) {
        break;
      }
      if (copy_array && ui_but_has_array_value(but)) {
        ui_but_copy_numeric_array(but, buf, buf_max_len);
      }
      else {
        ui_but_copy_numeric_value(but, buf, buf_max_len);
      }
      is_buf_set = true;
      break;

    case UI_BTYPE_UNITVEC:
      if (!has_required_data) {
        break;
      }
      ui_but_copy_numeric_array(but, buf, buf_max_len);
      is_buf_set = true;
      break;

    case UI_BTYPE_COLOR:
      if (!has_required_data) {
        break;
      }
      ui_but_copy_color(but, buf, buf_max_len);
      is_buf_set = true;
      break;

    case UI_BTYPE_TEXT:
    case UI_BTYPE_SEARCH_MENU:
      if (!has_required_data) {
        break;
      }
      ui_but_copy_text(but, buf, buf_max_len);
      is_buf_set = true;
      break;

    case UI_BTYPE_COLORBAND:
      ui_but_copy_colorband(but);
      break;

    case UI_BTYPE_CURVE:
      ui_but_copy_curvemapping(but);
      break;

    case UI_BTYPE_BUT:
      ui_but_copy_operator(C, but, buf, buf_max_len);
      is_buf_set = true;
      break;

    case UI_BTYPE_MENU:
    case UI_BTYPE_PULLDOWN:
      if (ui_but_copy_menu(but, buf, buf_max_len)) {
        is_buf_set = true;
      }
      break;
    case UI_BTYPE_POPOVER:
      if (ui_but_copy_popover(but, buf, buf_max_len)) {
        is_buf_set = true;
      }
      break;

    default:
      break;
  }

  if (is_buf_set) {
    WM_clipboard_text_set(buf, 0);
  }
}

static void ui_but_paste(bContext *C, uiBut *but, uiHandleButtonData *data, const bool paste_array)
{
  BLI_assert((but->flag & UI_BUT_DISABLED) == 0); /* caller should check */

  int buf_paste_len = 0;
  char *buf_paste;
  ui_but_get_pasted_text_from_clipboard(&buf_paste, &buf_paste_len);

  bool has_required_data = !(but->poin == NULL && but->rnapoin.data == NULL);

  switch (but->type) {
    case UI_BTYPE_NUM:
    case UI_BTYPE_NUM_SLIDER:
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

    case UI_BTYPE_UNITVEC:
      if (!has_required_data) {
        break;
      }
      ui_but_paste_normalized_vector(C, but, buf_paste);
      break;

    case UI_BTYPE_COLOR:
      if (!has_required_data) {
        break;
      }
      ui_but_paste_color(C, but, buf_paste);
      break;

    case UI_BTYPE_TEXT:
    case UI_BTYPE_SEARCH_MENU:
      if (!has_required_data) {
        break;
      }
      ui_but_paste_text(C, but, data, buf_paste);
      break;

    case UI_BTYPE_COLORBAND:
      ui_but_paste_colorband(C, but, data);
      break;

    case UI_BTYPE_CURVE:
      ui_but_paste_curvemapping(C, but);
      break;

    default:
      break;
  }

  MEM_freeN((void *)buf_paste);
}

void ui_but_clipboard_free(void)
{
  curvemapping_free_data(&but_copypaste_curve);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Text Password
 *
 * Functions to convert password strings that should not be displayed
 * to asterisk representation (e.g. 'mysecretpasswd' -> '*************')
 *
 * It converts every UTF-8 character to an asterisk, and also remaps
 * the cursor position and selection start/end.
 *
 * \note: remapping is used, because password could contain UTF-8 characters.
 *
 * \{ */

static int ui_text_position_from_hidden(uiBut *but, int pos)
{
  const char *strpos, *butstr;
  int i;

  butstr = (but->editstr) ? but->editstr : but->drawstr;

  for (i = 0, strpos = butstr; i < pos; i++) {
    strpos = BLI_str_find_next_char_utf8(strpos, NULL);
  }

  return (strpos - butstr);
}

static int ui_text_position_to_hidden(uiBut *but, int pos)
{
  const char *butstr = (but->editstr) ? but->editstr : but->drawstr;
  return BLI_strnlen_utf8(butstr, pos);
}

void ui_but_text_password_hide(char password_str[UI_MAX_PASSWORD_STR],
                               uiBut *but,
                               const bool restore)
{
  char *butstr;

  if (!(but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_PASSWORD)) {
    return;
  }

  butstr = (but->editstr) ? but->editstr : but->drawstr;

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

static void ui_textedit_string_clear_and_exit(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  /* most likely NULL, but let's check, and give it temp zero string */
  if (!data->str) {
    data->str = MEM_callocN(1, "temp str");
  }
  data->str[0] = 0;

  ui_apply_but_TEX(C, but, data);
  button_activate_state(C, but, BUTTON_STATE_EXIT);
}

static void ui_textedit_string_ensure_max_length(uiBut *but, uiHandleButtonData *data, int maxlen)
{
  BLI_assert(data->is_str_dynamic);
  BLI_assert(data->str == but->editstr);

  if (maxlen > data->maxlen) {
    data->str = but->editstr = MEM_reallocN(data->str, sizeof(char) * maxlen);
    data->maxlen = maxlen;
  }
}

static void ui_textedit_string_set(uiBut *but, uiHandleButtonData *data, const char *str)
{
  if (data->is_str_dynamic) {
    ui_textedit_string_ensure_max_length(but, data, strlen(str) + 1);
  }

  if (ui_but_is_utf8(but)) {
    BLI_strncpy_utf8(data->str, str, data->maxlen);
  }
  else {
    BLI_strncpy(data->str, str, data->maxlen);
  }
}

static bool ui_textedit_delete_selection(uiBut *but, uiHandleButtonData *data)
{
  char *str = data->str;
  const int len = strlen(str);
  bool changed = false;
  if (but->selsta != but->selend && len) {
    memmove(str + but->selsta, str + but->selend, (len - but->selend) + 1);
    changed = true;
  }

  but->pos = but->selend = but->selsta;
  return changed;
}

/**
 * \param x: Screen space cursor location - #wmEvent.x
 *
 * \note ``but->block->aspect`` is used here, so drawing button style is getting scaled too.
 */
static void ui_textedit_set_cursor_pos(uiBut *but, uiHandleButtonData *data, const float x)
{
  uiStyle *style = UI_style_get();  // XXX pass on as arg
  uiFontStyle *fstyle = &style->widget;
  const float aspect = but->block->aspect;
  const short fstyle_points_prev = fstyle->points;

  float startx = but->rect.xmin;
  float starty_dummy = 0.0f;
  char password_str[UI_MAX_PASSWORD_STR];
  /* treat 'str_last' as null terminator for str, no need to modify in-place */
  const char *str = but->editstr, *str_last;

  ui_block_to_window_fl(data->region, but->block, &startx, &starty_dummy);

  ui_fontscale(&fstyle->points, aspect);

  UI_fontstyle_set(fstyle);

  if (fstyle->kerning == 1) {
    /* for BLF_width */
    BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  ui_but_text_password_hide(password_str, but, false);

  if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
    if (but->flag & UI_HAS_ICON) {
      startx += UI_DPI_ICON_SIZE / aspect;
    }
  }
  /* but this extra .05 makes clicks inbetween characters feel nicer */
  startx += ((UI_TEXT_MARGIN_X + 0.05f) * U.widget_unit) / aspect;

  /* mouse dragged outside the widget to the left */
  if (x < startx) {
    int i = but->ofs;

    str_last = &str[but->ofs];

    while (i > 0) {
      if (BLI_str_cursor_step_prev_utf8(str, but->ofs, &i)) {
        /* 0.25 == scale factor for less sensitivity */
        if (BLF_width(fstyle->uifont_id, str + i, (str_last - str) - i) > (startx - x) * 0.25f) {
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
  else { /* (x >= startx) */
    int pos_i;

    /* keep track of previous distance from the cursor to the char */
    float cdist, cdist_prev = 0.0f;
    short pos_prev;

    str_last = &str[strlen(str)];

    but->pos = pos_prev = ((str_last - str) - but->ofs);

    while (true) {
      cdist = startx + BLF_width(fstyle->uifont_id, str + but->ofs, (str_last - str) - but->ofs);

      /* check if position is found */
      if (cdist < x) {
        /* check is previous location was in fact closer */
        if ((x - cdist) > (cdist_prev - x)) {
          but->pos = pos_prev;
        }
        break;
      }
      cdist_prev = cdist;
      pos_prev = but->pos;
      /* done with tricky distance checks */

      pos_i = but->pos;
      if (but->pos <= 0) {
        break;
      }
      if (BLI_str_cursor_step_prev_utf8(str, but->ofs, &pos_i)) {
        but->pos = pos_i;
        str_last = &str[but->pos + but->ofs];
      }
      else {
        break; /* unlikely but possible */
      }
    }
    but->pos += but->ofs;
    if (but->pos < 0) {
      but->pos = 0;
    }
  }

  if (fstyle->kerning == 1) {
    BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  ui_but_text_password_hide(password_str, but, true);

  fstyle->points = fstyle_points_prev;
}

static void ui_textedit_set_cursor_select(uiBut *but, uiHandleButtonData *data, const float x)
{
  if (x > data->selstartx) {
    data->selextend = EXTEND_RIGHT;
  }
  else if (x < data->selstartx) {
    data->selextend = EXTEND_LEFT;
  }

  ui_textedit_set_cursor_pos(but, data, x);

  if (data->selextend == EXTEND_RIGHT) {
    but->selend = but->pos;
  }
  else if (data->selextend == EXTEND_LEFT) {
    but->selsta = but->pos;
  }

  ui_but_update(but);
}

/**
 * This is used for both utf8 and ascii
 *
 * For unicode buttons, \a buf is treated as unicode.
 */
static bool ui_textedit_insert_buf(uiBut *but,
                                   uiHandleButtonData *data,
                                   const char *buf,
                                   int buf_len)
{
  int len = strlen(data->str);
  int len_new = len - (but->selend - but->selsta) + 1;
  bool changed = false;

  if (data->is_str_dynamic) {
    ui_textedit_string_ensure_max_length(but, data, len_new + buf_len);
  }

  if (len_new <= data->maxlen) {
    char *str = data->str;
    size_t step = buf_len;

    /* type over the current selection */
    if ((but->selend - but->selsta) > 0) {
      changed = ui_textedit_delete_selection(but, data);
      len = strlen(str);
    }

    if ((len + step >= data->maxlen) && (data->maxlen - (len + 1) > 0)) {
      if (ui_but_is_utf8(but)) {
        /* shorten 'step' to a utf8 aligned size that fits  */
        BLI_strnlen_utf8_ex(buf, data->maxlen - (len + 1), &step);
      }
      else {
        step = data->maxlen - (len + 1);
      }
    }

    if (step && (len + step < data->maxlen)) {
      memmove(&str[but->pos + step], &str[but->pos], (len + 1) - but->pos);
      memcpy(&str[but->pos], buf, step * sizeof(char));
      but->pos += step;
      changed = true;
    }
  }

  return changed;
}

static bool ui_textedit_insert_ascii(uiBut *but, uiHandleButtonData *data, char ascii)
{
  char buf[2] = {ascii, '\0'};

  if (ui_but_is_utf8(but) && (BLI_str_utf8_size(buf) == -1)) {
    printf(
        "%s: entering invalid ascii char into an ascii key (%d)\n", __func__, (int)(uchar)ascii);

    return false;
  }

  /* in some cases we want to allow invalid utf8 chars */
  return ui_textedit_insert_buf(but, data, buf, 1);
}

static void ui_textedit_move(uiBut *but,
                             uiHandleButtonData *data,
                             eStrCursorJumpDirection direction,
                             const bool select,
                             eStrCursorJumpType jump)
{
  const char *str = data->str;
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
    data->selextend = EXTEND_NONE;
  }
  else {
    int pos_i = but->pos;
    BLI_str_cursor_step_utf8(str, len, &pos_i, direction, jump, true);
    but->pos = pos_i;

    if (select) {
      /* existing selection */
      if (has_sel) {

        if (data->selextend == EXTEND_NONE) {
          data->selextend = EXTEND_RIGHT;
        }

        if (direction) {
          if (data->selextend == EXTEND_RIGHT) {
            but->selend = but->pos;
          }
          else {
            but->selsta = but->pos;
          }
        }
        else {
          if (data->selextend == EXTEND_LEFT) {
            but->selsta = but->pos;
          }
          else {
            but->selend = but->pos;
          }
        }

        if (but->selend < but->selsta) {
          SWAP(short, but->selsta, but->selend);
          data->selextend = (data->selextend == EXTEND_RIGHT) ? EXTEND_LEFT : EXTEND_RIGHT;
        }

      } /* new selection */
      else {
        if (direction) {
          data->selextend = EXTEND_RIGHT;
          but->selend = but->pos;
          but->selsta = pos_prev;
        }
        else {
          data->selextend = EXTEND_LEFT;
          but->selend = pos_prev;
          but->selsta = but->pos;
        }
      }
    }
  }
}

static bool ui_textedit_delete(uiBut *but,
                               uiHandleButtonData *data,
                               int direction,
                               eStrCursorJumpType jump)
{
  char *str = data->str;
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
      changed = ui_textedit_delete_selection(but, data);
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
        changed = ui_textedit_delete_selection(but, data);
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
  char *str;
  int changed;

  str = data->str;

  if (data->searchbox) {
    changed = ui_searchbox_autocomplete(C, data->searchbox, but, data->str);
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

static bool ui_textedit_copypaste(uiBut *but, uiHandleButtonData *data, const int mode)
{
  char *pbuf;
  bool changed = false;
  int buf_len;

  /* paste */
  if (mode == UI_TEXTEDIT_PASTE) {
    /* extract the first line from the clipboard */
    pbuf = WM_clipboard_text_get_firstline(false, &buf_len);

    if (pbuf) {
      if (ui_but_is_utf8(but)) {
        buf_len -= BLI_utf8_invalid_strip(pbuf, (size_t)buf_len);
      }

      ui_textedit_insert_buf(but, data, pbuf, buf_len);

      changed = true;

      MEM_freeN(pbuf);
    }
  }
  /* cut & copy */
  else if (ELEM(mode, UI_TEXTEDIT_COPY, UI_TEXTEDIT_CUT)) {
    /* copy the contents to the copypaste buffer */
    int sellen = but->selend - but->selsta;
    char *buf = MEM_mallocN(sizeof(char) * (sellen + 1), "ui_textedit_copypaste");

    BLI_strncpy(buf, data->str + but->selsta, sellen + 1);
    WM_clipboard_text_set(buf, 0);
    MEM_freeN(buf);

    /* for cut only, delete the selection afterwards */
    if (mode == UI_TEXTEDIT_CUT) {
      if ((but->selend - but->selsta) > 0) {
        changed = ui_textedit_delete_selection(but, data);
      }
    }
  }

  return changed;
}

#ifdef WITH_INPUT_IME
/* enable ime, and set up uibut ime data */
static void ui_textedit_ime_begin(wmWindow *win, uiBut *UNUSED(but))
{
  /* XXX Is this really needed? */
  int x, y;

  BLI_assert(win->ime_data == NULL);

  /* enable IME and position to cursor, it's a trick */
  x = win->eventstate->x;
  /* flip y and move down a bit, prevent the IME panel cover the edit button */
  y = win->eventstate->y - 12;

  wm_window_IME_begin(win, x, y, 0, 0, true);
}

/* disable ime, and clear uibut ime data */
static void ui_textedit_ime_end(wmWindow *win, uiBut *UNUSED(but))
{
  wm_window_IME_end(win);
}

void ui_but_ime_reposition(uiBut *but, int x, int y, bool complete)
{
  BLI_assert(but->active);

  ui_region_to_window(but->active->region, &x, &y);
  wm_window_IME_begin(but->active->window, x, y - 4, 0, 0, complete);
}

wmIMEData *ui_but_ime_data_get(uiBut *but)
{
  if (but->active && but->active->window) {
    return but->active->window->ime_data;
  }
  else {
    return NULL;
  }
}
#endif /* WITH_INPUT_IME */

static void ui_textedit_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  wmWindow *win = CTX_wm_window(C);
  int len;
  const bool is_num_but = ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER);
  bool no_zero_strip = false;

  if (data->str) {
    MEM_freeN(data->str);
    data->str = NULL;
  }

#ifdef USE_DRAG_MULTINUM
  /* this can happen from multi-drag */
  if (data->applied_interactive) {
    /* remove any small changes so canceling edit doesn't restore invalid value: T40538 */
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
  data->maxlen = ui_but_string_get_max_length(but);
  if (data->maxlen != 0) {
    data->str = MEM_callocN(sizeof(char) * data->maxlen, "textedit str");
    /* We do not want to truncate precision to default here, it's nice to show value,
     * not to edit it - way too much precision is lost then. */
    ui_but_string_get_ex(
        but, data->str, data->maxlen, UI_PRECISION_FLOAT_MAX, true, &no_zero_strip);
  }
  else {
    data->is_str_dynamic = true;
    data->str = ui_but_string_get_dynamic(but, &data->maxlen);
  }

  if (ui_but_is_float(but) && !ui_but_is_unit(but) && !ui_but_anim_expression_get(but, NULL, 0) &&
      !no_zero_strip) {
    BLI_str_rstrip_float_zero(data->str, '\0');
  }

  if (is_num_but) {
    BLI_assert(data->is_str_dynamic == false);
    ui_but_convert_to_unit_alt_name(but, data->str, data->maxlen);
  }

  /* won't change from now on */
  len = strlen(data->str);

  data->origstr = BLI_strdupn(data->str, len);
  data->selextend = EXTEND_NONE;
  data->selstartx = 0.0f;

  /* set cursor pos to the end of the text */
  but->editstr = data->str;
  but->pos = len;
  but->selsta = 0;
  but->selend = len;

  /* optional searchbox */
  if (but->type == UI_BTYPE_SEARCH_MENU) {
    data->searchbox = but->search_create_func(C, data->region, but);
    ui_searchbox_update(C, data->searchbox, but, true); /* true = reset */
  }

  /* reset alert flag (avoid confusion, will refresh on exit) */
  but->flag &= ~UI_BUT_REDALERT;

  ui_but_update(but);

  WM_cursor_modal_set(win, BC_TEXTEDITCURSOR);

#ifdef WITH_INPUT_IME
  if (is_num_but == false && BLT_lang_is_ime_supported()) {
    ui_textedit_ime_begin(win, but);
  }
#endif
}

static void ui_textedit_end(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  wmWindow *win = CTX_wm_window(C);

  if (but) {
    if (ui_but_is_utf8(but)) {
      int strip = BLI_utf8_invalid_strip(but->editstr, strlen(but->editstr));
      /* not a file?, strip non utf-8 chars */
      if (strip) {
        /* wont happen often so isn't that annoying to keep it here for a while */
        printf("%s: invalid utf8 - stripped chars %d\n", __func__, strip);
      }
    }

    if (data->searchbox) {
      if (data->cancel == false) {
        if ((ui_searchbox_apply(but, data->searchbox) == false) &&
            (ui_searchbox_find_index(data->searchbox, but->editstr) == -1)) {
          data->cancel = true;

          /* ensure menu (popup) too is closed! */
          data->escapecancel = true;
        }
      }

      ui_searchbox_free(C, data->searchbox);
      data->searchbox = NULL;
    }

    but->editstr = NULL;
    but->pos = -1;
  }

  WM_cursor_modal_restore(win);

#ifdef WITH_INPUT_IME
  if (win->ime_data) {
    ui_textedit_ime_end(win, but);
  }
#endif
}

static void ui_textedit_next_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
  uiBut *but;

  /* label and roundbox can overlap real buttons (backdrops...) */
  if (ELEM(actbut->type,
           UI_BTYPE_LABEL,
           UI_BTYPE_SEPR,
           UI_BTYPE_SEPR_LINE,
           UI_BTYPE_ROUNDBOX,
           UI_BTYPE_LISTBOX)) {
    return;
  }

  for (but = actbut->next; but; but = but->next) {
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & UI_BUT_DISABLED)) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
  for (but = block->buttons.first; but != actbut; but = but->next) {
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & UI_BUT_DISABLED)) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
}

static void ui_textedit_prev_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
  uiBut *but;

  /* label and roundbox can overlap real buttons (backdrops...) */
  if (ELEM(actbut->type,
           UI_BTYPE_LABEL,
           UI_BTYPE_SEPR,
           UI_BTYPE_SEPR_LINE,
           UI_BTYPE_ROUNDBOX,
           UI_BTYPE_LISTBOX)) {
    return;
  }

  for (but = actbut->prev; but; but = but->prev) {
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & UI_BUT_DISABLED)) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
  for (but = block->buttons.last; but != actbut; but = but->prev) {
    if (ui_but_is_editable_as_text(but)) {
      if (!(but->flag & UI_BUT_DISABLED)) {
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
        return;
      }
    }
  }
}

static void ui_do_but_textedit(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int retval = WM_UI_HANDLER_CONTINUE;
  bool changed = false, inbox = false, update = false;

#ifdef WITH_INPUT_IME
  wmWindow *win = CTX_wm_window(C);
  wmIMEData *ime_data = win->ime_data;
  bool is_ime_composing = ime_data && ime_data->is_ime_composing;
#else
  bool is_ime_composing = false;
#endif

  switch (event->type) {
    case MOUSEMOVE:
    case MOUSEPAN:
      if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
        if ((event->type == MOUSEMOVE) &&
            ui_mouse_motion_keynav_test(&data->searchbox_keynav_state, event)) {
          /* pass */
        }
        else {
          ui_searchbox_event(C, data->searchbox, but, event);
        }
#else
        ui_searchbox_event(C, data->searchbox, but, event);
#endif
      }

      break;
    case RIGHTMOUSE:
    case ESCKEY:
      if (event->val == KM_PRESS) {
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
      bool had_selection = but->selsta != but->selend;

      /* exit on LMB only on RELEASE for searchbox, to mimic other popups,
       * and allow multiple menu levels */
      if (data->searchbox) {
        inbox = ui_searchbox_inside(data->searchbox, event->x, event->y);
      }

      /* for double click: we do a press again for when you first click on button
       * (selects all text, no cursor pos) */
      if (event->val == KM_PRESS || event->val == KM_DBL_CLICK) {
        float mx, my;

        mx = event->x;
        my = event->y;
        ui_window_to_block_fl(data->region, block, &mx, &my);

        if (ui_but_contains_pt(but, mx, my)) {
          ui_textedit_set_cursor_pos(but, data, event->x);
          but->selsta = but->selend = but->pos;
          data->selstartx = event->x;

          button_activate_state(C, but, BUTTON_STATE_TEXT_SELECTING);
          retval = WM_UI_HANDLER_BREAK;
        }
        else if (inbox == false) {
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
        ui_textedit_move(but, data, STRCUR_DIR_PREV, false, STRCUR_JUMP_DELIM);
        ui_textedit_move(but, data, STRCUR_DIR_NEXT, true, STRCUR_JUMP_DELIM);
        retval = WM_UI_HANDLER_BREAK;
        changed = true;
      }
      else if (inbox) {
        /* if we allow activation on key press,
         * it gives problems launching operators T35713. */
        if (event->val == KM_RELEASE) {
          button_activate_state(C, but, BUTTON_STATE_EXIT);
          retval = WM_UI_HANDLER_BREAK;
        }
      }
      break;
    }
  }

  if (event->val == KM_PRESS && !is_ime_composing) {
    switch (event->type) {
      case VKEY:
      case XKEY:
      case CKEY:
        if (IS_EVENT_MOD(event, ctrl, oskey)) {
          if (event->type == VKEY) {
            changed = ui_textedit_copypaste(but, data, UI_TEXTEDIT_PASTE);
          }
          else if (event->type == CKEY) {
            changed = ui_textedit_copypaste(but, data, UI_TEXTEDIT_COPY);
          }
          else if (event->type == XKEY) {
            changed = ui_textedit_copypaste(but, data, UI_TEXTEDIT_CUT);
          }

          retval = WM_UI_HANDLER_BREAK;
        }
        break;
      case RIGHTARROWKEY:
        ui_textedit_move(but,
                         data,
                         STRCUR_DIR_NEXT,
                         event->shift != 0,
                         event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case LEFTARROWKEY:
        ui_textedit_move(but,
                         data,
                         STRCUR_DIR_PREV,
                         event->shift != 0,
                         event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case WHEELDOWNMOUSE:
      case DOWNARROWKEY:
        if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
          ui_mouse_motion_keynav_init(&data->searchbox_keynav_state, event);
#endif
          ui_searchbox_event(C, data->searchbox, but, event);
          break;
        }
        if (event->type == WHEELDOWNMOUSE) {
          break;
        }
        ATTR_FALLTHROUGH;
      case ENDKEY:
        ui_textedit_move(but, data, STRCUR_DIR_NEXT, event->shift != 0, STRCUR_JUMP_ALL);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case WHEELUPMOUSE:
      case UPARROWKEY:
        if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
          ui_mouse_motion_keynav_init(&data->searchbox_keynav_state, event);
#endif
          ui_searchbox_event(C, data->searchbox, but, event);
          break;
        }
        if (event->type == WHEELUPMOUSE) {
          break;
        }
        ATTR_FALLTHROUGH;
      case HOMEKEY:
        ui_textedit_move(but, data, STRCUR_DIR_PREV, event->shift != 0, STRCUR_JUMP_ALL);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case PADENTER:
      case RETKEY:
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
        break;
      case DELKEY:
        changed = ui_textedit_delete(
            but, data, 1, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
        retval = WM_UI_HANDLER_BREAK;
        break;

      case BACKSPACEKEY:
        changed = ui_textedit_delete(
            but, data, 0, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
        retval = WM_UI_HANDLER_BREAK;
        break;

      case AKEY:

        /* Ctrl + A: Select all */
#if defined(__APPLE__)
        /* OSX uses cmd-a systemwide, so add it */
        if ((event->oskey && !IS_EVENT_MOD(event, shift, alt, ctrl)) ||
            (event->ctrl && !IS_EVENT_MOD(event, shift, alt, oskey)))
#else
        if (event->ctrl && !IS_EVENT_MOD(event, shift, alt, oskey))
#endif
        {
          ui_textedit_move(but, data, STRCUR_DIR_PREV, false, STRCUR_JUMP_ALL);
          ui_textedit_move(but, data, STRCUR_DIR_NEXT, true, STRCUR_JUMP_ALL);
          retval = WM_UI_HANDLER_BREAK;
        }
        break;

      case TABKEY:
        /* there is a key conflict here, we can't tab with autocomplete */
        if (but->autocomplete_func || data->searchbox) {
          int autocomplete = ui_textedit_autocomplete(C, but, data);
          changed = autocomplete != AUTOCOMPLETE_NO_MATCH;

          if (autocomplete == AUTOCOMPLETE_FULL_MATCH) {
            button_activate_state(C, but, BUTTON_STATE_EXIT);
          }
        }
        /* the hotkey here is not well defined, was G.qual so we check all */
        else if (IS_EVENT_MOD(event, shift, ctrl, alt, oskey)) {
          ui_textedit_prev_but(block, but, data);
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        else {
          ui_textedit_next_but(block, but, data);
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        retval = WM_UI_HANDLER_BREAK;
        break;
    }

    if ((event->ascii || event->utf8_buf[0]) && (retval == WM_UI_HANDLER_CONTINUE)
#ifdef WITH_INPUT_IME
        && !is_ime_composing && (!WM_event_is_ime_switch(event) || !BLT_lang_is_ime_supported())
#endif
    ) {
      char ascii = event->ascii;
      const char *utf8_buf = event->utf8_buf;

      /* exception that's useful for number buttons, some keyboard
       * numpads have a comma instead of a period */
      if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) { /* could use data->min*/
        if (event->type == PADPERIOD && ascii == ',') {
          ascii = '.';
          utf8_buf = NULL; /* force ascii fallback */
        }
      }

      if (utf8_buf && utf8_buf[0]) {
        int utf8_buf_len = BLI_str_utf8_size(utf8_buf);
        /* keep this printf until utf8 is well tested */
        if (utf8_buf_len != 1) {
          printf("%s: utf8 char '%.*s'\n", __func__, utf8_buf_len, utf8_buf);
        }

        // strcpy(utf8_buf, "12345");
        changed = ui_textedit_insert_buf(but, data, event->utf8_buf, utf8_buf_len);
      }
      else {
        changed = ui_textedit_insert_ascii(but, data, ascii);
      }

      retval = WM_UI_HANDLER_BREAK;
    }
    /* textbutton with this flag: do live update (e.g. for search buttons) */
    if (but->flag & UI_BUT_TEXTEDIT_UPDATE) {
      update = true;
    }
  }

#ifdef WITH_INPUT_IME
  if (event->type == WM_IME_COMPOSITE_START || event->type == WM_IME_COMPOSITE_EVENT) {
    changed = true;

    if (event->type == WM_IME_COMPOSITE_START && but->selend > but->selsta) {
      ui_textedit_delete_selection(but, data);
    }
    if (event->type == WM_IME_COMPOSITE_EVENT && ime_data->result_len) {
      ui_textedit_insert_buf(but, data, ime_data->str_result, ime_data->result_len);
    }
  }
  else if (event->type == WM_IME_COMPOSITE_END) {
    changed = true;
  }
#endif

  if (changed) {
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
  }
}

static void ui_do_but_textedit_select(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx, my, retval = WM_UI_HANDLER_CONTINUE;

  switch (event->type) {
    case MOUSEMOVE: {
      mx = event->x;
      my = event->y;
      ui_window_to_block(data->region, block, &mx, &my);

      ui_textedit_set_cursor_select(but, data, event->x);
      retval = WM_UI_HANDLER_BREAK;
      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      }
      retval = WM_UI_HANDLER_BREAK;
      break;
  }

  if (retval == WM_UI_HANDLER_BREAK) {
    ui_but_update(but);
    ED_region_tag_redraw(data->region);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Number Editing (various types)
 * \{ */

static void ui_numedit_begin(uiBut *but, uiHandleButtonData *data)
{
  if (but->type == UI_BTYPE_CURVE) {
    but->editcumap = (CurveMapping *)but->poin;
  }
  else if (but->type == UI_BTYPE_COLORBAND) {
    data->coba = (ColorBand *)but->poin;
    but->editcoba = data->coba;
  }
  else if (ELEM(but->type,
                UI_BTYPE_UNITVEC,
                UI_BTYPE_HSVCUBE,
                UI_BTYPE_HSVCIRCLE,
                UI_BTYPE_COLOR)) {
    ui_but_v3_get(but, data->origvec);
    copy_v3_v3(data->vec, data->origvec);
    but->editvec = data->vec;
  }
  else {
    float softrange, softmin, softmax;

    data->startvalue = ui_but_value_get(but);
    data->origvalue = data->startvalue;
    data->value = data->origvalue;
    but->editval = &data->value;

    softmin = but->softmin;
    softmax = but->softmax;
    softrange = softmax - softmin;

    data->dragfstart = (softrange == 0.0f) ? 0.0f : ((float)data->value - softmin) / softrange;
    data->dragf = data->dragfstart;
  }

  data->dragchange = false;
  data->draglock = true;
}

static void ui_numedit_end(uiBut *but, uiHandleButtonData *data)
{
  but->editval = NULL;
  but->editvec = NULL;
  but->editcoba = NULL;
  but->editcumap = NULL;

  data->dragstartx = 0;
  data->draglastx = 0;
  data->dragchange = false;
  data->dragcbd = NULL;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu/Popup Begin/End (various popup types)
 * \{ */

static void ui_block_open_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  uiBlockCreateFunc func = NULL;
  uiBlockHandleCreateFunc handlefunc = NULL;
  uiMenuCreateFunc menufunc = NULL;
  uiMenuCreateFunc popoverfunc = NULL;
  void *arg = NULL;

  switch (but->type) {
    case UI_BTYPE_BLOCK:
    case UI_BTYPE_PULLDOWN:
      if (but->menu_create_func) {
        menufunc = but->menu_create_func;
        arg = but->poin;
      }
      else {
        func = but->block_create_func;
        arg = but->poin ? but->poin : but->func_argN;
      }
      break;
    case UI_BTYPE_MENU:
    case UI_BTYPE_POPOVER:
      BLI_assert(but->menu_create_func);
      if ((but->type == UI_BTYPE_POPOVER) || ui_but_menu_draw_as_popover(but)) {
        popoverfunc = but->menu_create_func;
      }
      else {
        menufunc = but->menu_create_func;
      }
      arg = but->poin;
      break;
    case UI_BTYPE_COLOR:
      ui_but_v3_get(but, data->origvec);
      copy_v3_v3(data->vec, data->origvec);
      but->editvec = data->vec;

      handlefunc = ui_block_func_COLOR;
      arg = but;
      break;

      /* quiet warnings for unhandled types */
    default:
      break;
  }

  if (func || handlefunc) {
    data->menu = ui_popup_block_create(C, data->region, but, func, handlefunc, arg, NULL);
    if (but->block->handle) {
      data->menu->popup = but->block->handle->popup;
    }
  }
  else if (menufunc) {
    data->menu = ui_popup_menu_create(C, data->region, but, menufunc, arg);
    if (but->block->handle) {
      data->menu->popup = but->block->handle->popup;
    }
  }
  else if (popoverfunc) {
    data->menu = ui_popover_panel_create(C, data->region, but, popoverfunc, arg);
    if (but->block->handle) {
      data->menu->popup = but->block->handle->popup;
    }
  }

#ifdef USE_ALLSELECT
  {
    wmWindow *win = CTX_wm_window(C);
    if (IS_ALLSELECT_EVENT(win->eventstate)) {
      data->select_others.is_enabled = true;
    }
  }
#endif

  /* this makes adjacent blocks auto open from now on */
  // if (but->block->auto_open == 0) {
  //  but->block->auto_open = 1;
  //}
}

static void ui_block_open_end(bContext *C, uiBut *but, uiHandleButtonData *data)
{
  if (but) {
    but->editval = NULL;
    but->editvec = NULL;

    but->block->auto_open_last = PIL_check_seconds_timer();
  }

  if (data->menu) {
    ui_popup_block_free(C, data->menu);
    data->menu = NULL;
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
 * Hack for #uiList #UI_BTYPE_LISTROW buttons to "give" events to overlaying #UI_BTYPE_TEXT buttons
 * (Ctrl-Click rename feature & co).
 */
static uiBut *ui_but_list_row_text_activate(bContext *C,
                                            uiBut *but,
                                            uiHandleButtonData *data,
                                            const wmEvent *event,
                                            uiButtonActivateType activate_type)
{
  ARegion *ar = CTX_wm_region(C);
  uiBut *labelbut = ui_but_find_mouse_over_ex(ar, event->x, event->y, true);

  if (labelbut && labelbut->type == UI_BTYPE_TEXT && !(labelbut->flag & UI_BUT_DISABLED)) {
    /* exit listrow */
    data->cancel = true;
    button_activate_exit(C, but, data, false, false);

    /* Activate the text button. */
    button_activate_init(C, ar, labelbut, activate_type);

    return labelbut;
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Events for Various Button Types
 * \{ */

#ifdef USE_DRAG_TOGGLE
/* Shared by any button that supports drag-toggle. */
static bool ui_do_but_ANY_drag_toggle(
    bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event, int *r_retval)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS && ui_but_is_drag_toggle(but)) {
#  if 0 /* UNUSED */
      data->togdual = event->ctrl;
      data->togonly = !event->shift;
#  endif
      ui_apply_but(C, but->block, but, data, true);
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->x;
      data->dragstarty = event->y;
      *r_retval = WM_UI_HANDLER_BREAK;
      return true;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_DRAG) {
    /* note: the 'BUTTON_STATE_WAIT_DRAG' part of 'ui_do_but_EXIT' could be refactored into
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
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE && but->block->handle) {
      /* regular buttons will be 'UI_SELECT', menu items 'UI_ACTIVE' */
      if (!(but->flag & (UI_SELECT | UI_ACTIVE))) {
        data->cancel = true;
      }
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
    else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
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
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
      but->drawstr[0] = 0;
      but->modifier_key = 0;
      button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
      return WM_UI_HANDLER_CONTINUE;
    }
    else if (event->type == UNKNOWNKEY) {
      return WM_UI_HANDLER_CONTINUE;
    }

    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      /* only cancel if click outside the button */
      if (ui_but_contains_point_px(but, but->active->region, event->x, event->y) == 0) {
        /* data->cancel doesn't work, this button opens immediate */
        if (but->flag & UI_BUT_IMMEDIATE) {
          ui_but_value_set(but, 0);
        }
        else {
          data->cancel = true;
        }
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        return WM_UI_HANDLER_BREAK;
      }
    }

    /* always set */
    but->modifier_key = 0;
    if (event->shift) {
      but->modifier_key |= KM_SHIFT;
    }
    if (event->alt) {
      but->modifier_key |= KM_ALT;
    }
    if (event->ctrl) {
      but->modifier_key |= KM_CTRL;
    }
    if (event->oskey) {
      but->modifier_key |= KM_OSKEY;
    }

    ui_but_update(but);
    ED_region_tag_redraw(data->region);

    if (event->val == KM_PRESS) {
      if (ISHOTKEY(event->type) && (event->type != ESCKEY)) {
        if (WM_key_event_string(event->type, false)[0]) {
          ui_but_value_set(but, event->type);
        }
        else {
          data->cancel = true;
        }

        button_activate_state(C, but, BUTTON_STATE_EXIT);
        return WM_UI_HANDLER_BREAK;
      }
      else if (event->type == ESCKEY) {
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
    if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
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

static bool ui_but_is_mouse_over_icon_extra(const ARegion *region,
                                            uiBut *but,
                                            const int mouse_xy[2])
{
  int x = mouse_xy[0], y = mouse_xy[1];
  rcti icon_rect;

  BLI_assert(ui_but_icon_extra_get(but) != UI_BUT_ICONEXTRA_NONE);

  ui_window_to_block(region, but->block, &x, &y);

  BLI_rcti_rctf_copy(&icon_rect, &but->rect);
  icon_rect.xmin = icon_rect.xmax - (BLI_rcti_size_y(&icon_rect));

  return BLI_rcti_isect_pt(&icon_rect, x, y);
}

static int ui_do_but_TAB(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  const bool is_property = (but->rnaprop != NULL);

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

    if (is_property && ELEM(rna_type, PROP_POINTER, PROP_STRING) && (but->custom_data != NULL) &&
        (event->type == LEFTMOUSE) && ((event->val == KM_DBL_CLICK) || event->ctrl)) {
      button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
      return WM_UI_HANDLER_BREAK;
    }
    else if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY)) {
      int event_val = (is_property) ? KM_PRESS : KM_CLICK;
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

static int ui_do_but_TEX(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, PADENTER, RETKEY) && event->val == KM_PRESS) {
      if (ELEM(event->type, PADENTER, RETKEY) && (!ui_but_is_utf8(but))) {
        /* pass - allow filesel, enter to execute */
      }
      else if (but->dt == UI_EMBOSS_NONE && !event->ctrl) {
        /* pass */
      }
      else {
        const bool has_icon_extra = ui_but_icon_extra_get(but) == UI_BUT_ICONEXTRA_CLEAR;

        if (has_icon_extra && ui_but_is_mouse_over_icon_extra(data->region, but, &event->x)) {
          ui_textedit_string_clear_and_exit(C, but, data);
        }
        else {
          button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        }
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

static int ui_do_but_SEARCH_UNLINK(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  const uiButExtraIconType extra_icon_type = ui_but_icon_extra_get(but);
  const bool has_icon_extra = (extra_icon_type != UI_BUT_ICONEXTRA_NONE);

  /* unlink icon is on right */
  if ((ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, PADENTER, RETKEY)) && (has_icon_extra == true) &&
      (ui_but_is_mouse_over_icon_extra(data->region, but, &event->x) == true)) {
    /* doing this on KM_PRESS calls eyedropper after clicking unlink icon */
    if (event->val == KM_RELEASE) {
      /* unlink */
      if (extra_icon_type == UI_BUT_ICONEXTRA_CLEAR) {
        ui_textedit_string_clear_and_exit(C, but, data);
      }
      /* eyedropper */
      else if (extra_icon_type == UI_BUT_ICONEXTRA_EYEDROPPER) {
        WM_operator_name_call(C, "UI_OT_eyedropper_id", WM_OP_INVOKE_DEFAULT, NULL);
      }
      else {
        BLI_assert(0);
      }
    }
    return WM_UI_HANDLER_BREAK;
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
    if (ELEM(event->type, PADENTER, RETKEY)) {
      if (event->val == KM_PRESS) {
        do_activate = true;
      }
    }
    else if (event->type == LEFTMOUSE) {
      if (ui_block_is_menu(but->block)) {
        /* Behave like other menu items. */
        do_activate = (event->val == KM_RELEASE);
      }
      else {
        do_activate = (event->val == KM_PRESS);
      }
    }

    if (do_activate) {
#if 0 /* UNUSED */
      data->togdual = event->ctrl;
      data->togonly = !event->shift;
#endif
      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
    else if (ELEM(event->type, WHEELDOWNMOUSE, WHEELUPMOUSE) && event->ctrl) {
      /* Support alt+wheel on expanded enum rows */
      if (but->type == UI_BTYPE_ROW) {
        const int direction = (event->type == WHEELDOWNMOUSE) ? -1 : 1;
        uiBut *but_select = ui_but_find_select_in_enum(but, direction);
        if (but_select) {
          uiBut *but_other = (direction == -1) ? but_select->next : but_select->prev;
          if (but_other && ui_but_find_select_in_enum__cmp(but, but_other)) {
            ARegion *ar = data->region;

            data->cancel = true;
            button_activate_exit(C, but, data, false, false);

            /* Activate the text button. */
            button_activate_init(C, ar, but_other, BUTTON_ACTIVATE_OVER);
            data = but_other->active;
            if (data) {
              ui_apply_but(C, but->block, but_other, but_other->active, true);
              button_activate_exit(C, but_other, data, false, false);

              /* restore active button */
              button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
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

static int ui_do_but_EXIT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{

  if (data->state == BUTTON_STATE_HIGHLIGHT) {

    /* first handle click on icondrag type button */
    if (event->type == LEFTMOUSE && but->dragpoin) {
      if (ui_but_contains_point_px_icon(but, data->region, event)) {

        /* tell the button to wait and keep checking further events to
         * see if it should start dragging */
        button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
        data->dragstartx = event->x;
        data->dragstarty = event->y;
        return WM_UI_HANDLER_CONTINUE;
      }
    }
#ifdef USE_DRAG_TOGGLE
    if (event->type == LEFTMOUSE && ui_but_is_drag_toggle(but)) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->x;
      data->dragstarty = event->y;
      return WM_UI_HANDLER_CONTINUE;
    }
#endif

    if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
      int ret = WM_UI_HANDLER_BREAK;
      /* XXX (a bit ugly) Special case handling for filebrowser drag button */
      if (but->dragpoin && but->imb && ui_but_contains_point_px_icon(but, data->region, event)) {
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
static float ui_numedit_apply_snapf(uiBut *but,
                                    float tempf,
                                    float softmin,
                                    float softmax,
                                    float softrange,
                                    const enum eSnapType snap)
{
  if (tempf == softmin || tempf == softmax || snap == SNAP_OFF) {
    /* pass */
  }
  else {
    float fac = 1.0f;

    if (ui_but_is_unit(but)) {
      UnitSettings *unit = but->block->unit;
      int unit_type = RNA_SUBTYPE_UNIT_VALUE(UI_but_unit_type_get(but));

      if (bUnit_IsValid(unit->system, unit_type)) {
        fac = (float)bUnit_BaseScalar(unit->system, unit_type);
        if (ELEM(unit_type, B_UNIT_LENGTH, B_UNIT_AREA, B_UNIT_VOLUME)) {
          fac /= unit->scale_length;
        }
      }
    }

    if (fac != 1.0f) {
      /* snap in unit-space */
      tempf /= fac;
      /* softmin /= fac; */ /* UNUSED */
      /* softmax /= fac; */ /* UNUSED */
      softrange /= fac;
    }

    /* workaround, too high snapping values */
    /* snapping by 10's for float buttons is quite annoying (location, scale...),
     * but allow for rotations */
    if (softrange >= 21.0f) {
      UnitSettings *unit = but->block->unit;
      int unit_type = UI_but_unit_type_get(but);
      if ((unit_type == PROP_UNIT_ROTATION) && (unit->system_rotation != USER_UNIT_ROT_RADIANS)) {
        /* pass (degrees)*/
      }
      else {
        softrange = 20.0f;
      }
    }

    if (snap == SNAP_ON) {
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
    else if (snap == SNAP_ON_SMALL) {
      if (softrange < 2.10f) {
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
      BLI_assert(0);
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
  if (temp == softmin || temp == softmax) {
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

static bool ui_numedit_but_NUM(uiBut *but,
                               uiHandleButtonData *data,
                               int mx,
                               const bool is_motion,
                               const enum eSnapType snap,
                               float fac)
{
  float deler, tempf, softmin, softmax, softrange;
  int lvalue, temp;
  bool changed = false;
  const bool is_float = ui_but_is_float(but);

  /* prevent unwanted drag adjustments, test motion so modifier keys refresh. */
  if ((is_motion || data->draglock) && (ui_but_dragedit_update_mval(data, mx) == false)) {
    return changed;
  }

  softmin = but->softmin;
  softmax = but->softmax;
  softrange = softmax - softmin;

  if (ui_but_is_cursor_warp(but)) {
    /* Mouse location isn't screen clamped to the screen so use a linear mapping
     * 2px == 1-int, or 1px == 1-ClickStep */
    if (is_float) {
      fac *= 0.01f * but->a1;
      tempf = (float)data->startvalue + ((float)(mx - data->dragstartx) * fac);
      tempf = ui_numedit_apply_snapf(but, tempf, softmin, softmax, softrange, snap);

#if 1 /* fake moving the click start, nicer for dragging back after passing the limit */
      if (tempf < softmin) {
        data->dragstartx -= (softmin - tempf) / fac;
        tempf = softmin;
      }
      else if (tempf > softmax) {
        data->dragstartx += (tempf - softmax) / fac;
        tempf = softmax;
      }
#else
      CLAMP(tempf, softmin, softmax);
#endif

      if (tempf != (float)data->value) {
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

      temp = data->startvalue + (((double)mx - data->dragstartx) * (double)fac);
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
      /* larger for ints, we dont need to fine tune them */
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
      non_linear_scale = (float)abs(mx - data->dragstartx) / non_linear_pixel_map;
    }
    else {
      non_linear_scale = 1.0f;
    }

    if (is_float == false) {
      /* at minimum, moving cursor 2 pixels should change an int button. */
      CLAMP_MIN(non_linear_scale, 0.5f * U.pixelsize);
    }

    data->dragf += (((float)(mx - data->draglastx)) / deler) * non_linear_scale;

    CLAMP(data->dragf, 0.0f, 1.0f);
    data->draglastx = mx;
    tempf = (softmin + data->dragf * softrange);

    if (!is_float) {
      temp = round_fl_to_int(tempf);

      temp = ui_numedit_apply_snap(temp, softmin, softmax, snap);

      CLAMP(temp, softmin, softmax);
      lvalue = (int)data->value;

      if (temp != lvalue) {
        data->dragchange = true;
        data->value = (double)temp;
        changed = true;
      }
    }
    else {
      temp = 0;
      tempf = ui_numedit_apply_snapf(but, tempf, softmin, softmax, softrange, snap);

      CLAMP(tempf, softmin, softmax);

      if (tempf != (float)data->value) {
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
  int oldflag = but->drawflag;
  but->drawflag &= ~(UI_BUT_ACTIVE_LEFT | UI_BUT_ACTIVE_RIGHT);

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
    int mx = data->window->eventstate->x;
    int my = data->window->eventstate->y;
    ui_window_to_block(data->region, but->block, &mx, &my);

    if (mx < (but->rect.xmin + handle_width)) {
      but->drawflag |= UI_BUT_ACTIVE_LEFT;
    }
    else if (mx > (but->rect.xmax - handle_width)) {
      but->drawflag |= UI_BUT_ACTIVE_RIGHT;
    }
  }

  /* Don't change the cursor once pressed. */
  if ((but->flag & UI_SELECT) == 0) {
    if ((but->drawflag & (UI_BUT_ACTIVE_LEFT)) || (but->drawflag & (UI_BUT_ACTIVE_RIGHT))) {
      if (data->changed_cursor) {
        WM_cursor_modal_restore(data->window);
        data->changed_cursor = false;
      }
    }
    else {
      if (data->changed_cursor == false) {
        WM_cursor_modal_set(data->window, CURSOR_X_MOVE);
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
  int mx, my;               /* mouse location scaled to fit the UI */
  int screen_mx, screen_my; /* mouse location kept at screen pixel coords */
  int click = 0;
  int retval = WM_UI_HANDLER_CONTINUE;

  mx = screen_mx = event->x;
  my = screen_my = event->y;

  ui_window_to_block(data->region, block, &mx, &my);
  ui_numedit_set_active(but);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    int type = event->type, val = event->val;

    if (type == MOUSEPAN) {
      ui_pan_to_scroll(event, &type, &val);
    }

    /* XXX hardcoded keymap check.... */
    if (type == MOUSEPAN && event->alt) {
      /* allow accumulating values, otherwise scrolling gets preference */
      retval = WM_UI_HANDLER_BREAK;
    }
    else if (type == WHEELDOWNMOUSE && event->ctrl) {
      mx = but->rect.xmin;
      but->drawflag &= ~UI_BUT_ACTIVE_RIGHT;
      but->drawflag |= UI_BUT_ACTIVE_LEFT;
      click = 1;
    }
    else if (type == WHEELUPMOUSE && event->ctrl) {
      mx = but->rect.xmax;
      but->drawflag &= ~UI_BUT_ACTIVE_LEFT;
      but->drawflag |= UI_BUT_ACTIVE_RIGHT;
      click = 1;
    }
    else if (event->val == KM_PRESS) {
      if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->ctrl) {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
      else if (event->type == LEFTMOUSE) {
        data->dragstartx = data->draglastx = ui_but_is_cursor_warp(but) ? screen_mx : mx;
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
      else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
        click = 1;
      }
      else if (event->type == MINUSKEY && event->val == KM_PRESS) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        data->value = -data->value;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
      }

#ifdef USE_DRAG_MULTINUM
      copy_v2_v2_int(data->multi_data.drag_start, &event->x);
#endif
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (data->dragchange) {
#ifdef USE_DRAG_MULTINUM
        /* if we started multibutton but didn't drag, then edit */
        if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
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
      if (event->shift) {
        fac /= 10.0f;
      }

      if (ui_numedit_but_NUM(
              but, data, (ui_but_is_cursor_warp(but) ? screen_mx : mx), is_motion, snap, fac)) {
        ui_numedit_apply(C, block, but, data);
      }
#ifdef USE_DRAG_MULTINUM
      else if (data->multi_data.has_mbuts) {
        if (data->multi_data.init == BUTTON_MULTI_INIT_ENABLE) {
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
    float tempf, softmin, softmax;
    int temp;

    softmin = but->softmin;
    softmax = but->softmax;

    if (!ui_but_is_float(but)) {
      if (but->drawflag & UI_BUT_ACTIVE_LEFT) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

        temp = (int)data->value - 1;
        if (temp >= softmin && temp <= softmax) {
          data->value = (double)temp;
        }
        else {
          data->cancel = true;
        }

        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
      else if (but->drawflag & UI_BUT_ACTIVE_RIGHT) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

        temp = (int)data->value + 1;
        if (temp >= softmin && temp <= softmax) {
          data->value = (double)temp;
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
      if (but->drawflag & UI_BUT_ACTIVE_LEFT) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

        tempf = (float)data->value - (UI_PRECISION_FLOAT_SCALE * but->a1);
        if (tempf < softmin) {
          tempf = softmin;
        }
        data->value = tempf;

        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
      else if (but->drawflag & UI_BUT_ACTIVE_RIGHT) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

        tempf = (float)data->value + (UI_PRECISION_FLOAT_SCALE * but->a1);
        if (tempf > softmax) {
          tempf = softmax;
        }
        data->value = tempf;

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
  if ((but->type != UI_BTYPE_SCROLL) && (is_motion || data->draglock) &&
      (ui_but_dragedit_update_mval(data, mx) == false)) {
    return changed;
  }

  softmin = but->softmin;
  softmax = but->softmax;
  softrange = softmax - softmin;

  /* yes, 'mx' as both x/y is intentional */
  ui_mouse_scale_warp(data, mx, mx, &mx_fl, &my_fl, shift);

  if (but->type == UI_BTYPE_NUM_SLIDER) {
    cursor_x_range = BLI_rctf_size_x(&but->rect);
  }
  else if (but->type == UI_BTYPE_SCROLL) {
    const float size = (is_horizontal) ? BLI_rctf_size_x(&but->rect) :
                                         -BLI_rctf_size_y(&but->rect);
    cursor_x_range = size * (but->softmax - but->softmin) /
                     (but->softmax - but->softmin + but->a1);
  }
  else {
    float offs = (BLI_rctf_size_y(&but->rect) / 2.0f);
    cursor_x_range = (BLI_rctf_size_x(&but->rect) - offs);
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

  tempf = softmin + f * softrange;
  temp = round_fl_to_int(tempf);

  if (snap) {
    if (tempf == softmin || tempf == softmax) {
      /* pass */
    }
    else if (ui_but_is_float(but)) {

      if (shift) {
        if (tempf == softmin || tempf == softmax) {
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

    if (tempf != (float)data->value) {
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
  int mx, my, click = 0;
  int retval = WM_UI_HANDLER_CONTINUE;

  mx = event->x;
  my = event->y;
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    int type = event->type, val = event->val;

    if (type == MOUSEPAN) {
      ui_pan_to_scroll(event, &type, &val);
    }

    /* XXX hardcoded keymap check.... */
    if (type == MOUSEPAN && event->alt) {
      /* allow accumulating values, otherwise scrolling gets preference */
      retval = WM_UI_HANDLER_BREAK;
    }
    else if (type == WHEELDOWNMOUSE && event->ctrl) {
      mx = but->rect.xmin;
      click = 2;
    }
    else if (type == WHEELUPMOUSE && event->ctrl) {
      mx = but->rect.xmax;
      click = 2;
    }
    else if (event->val == KM_PRESS) {
      if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->ctrl) {
        button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
#ifndef USE_ALLSELECT
      /* alt-click on sides to get "arrows" like in UI_BTYPE_NUM buttons,
       * and match wheel usage above */
      else if (event->type == LEFTMOUSE && event->alt) {
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
      else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
        click = 1;
      }
      else if (event->type == MINUSKEY && event->val == KM_PRESS) {
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        data->value = -data->value;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
        retval = WM_UI_HANDLER_BREAK;
      }
    }
#ifdef USE_DRAG_MULTINUM
    copy_v2_v2_int(data->multi_data.drag_start, &event->x);
#endif
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
      if (data->dragchange) {
#ifdef USE_DRAG_MULTINUM
        /* if we started multibutton but didn't drag, then edit */
        if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
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
      if (ui_numedit_but_SLI(
              but, data, mx, true, is_motion, event->ctrl != 0, event->shift != 0)) {
        ui_numedit_apply(C, block, but, data);
      }

#ifdef USE_DRAG_MULTINUM
      else if (data->multi_data.has_mbuts) {
        if (data->multi_data.init == BUTTON_MULTI_INIT_ENABLE) {
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
      /* nudge slider to the left or right */
      float f, tempf, softmin, softmax, softrange;
      int temp;

      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      softmin = but->softmin;
      softmax = but->softmax;
      softrange = softmax - softmin;

      tempf = data->value;
      temp = (int)data->value;

#if 0
      if (but->type == SLI) {
        /* same as below */
        f = (float)(mx - but->rect.xmin) / (BLI_rctf_size_x(&but->rect));
      }
      else
#endif
      {
        f = (float)(mx - but->rect.xmin) / (BLI_rctf_size_x(&but->rect));
      }

      f = softmin + f * softrange;

      if (!ui_but_is_float(but)) {
        if (f < temp) {
          temp--;
        }
        else {
          temp++;
        }

        if (temp >= softmin && temp <= softmax) {
          data->value = temp;
        }
        else {
          data->cancel = true;
        }
      }
      else {
        if (f < tempf) {
          tempf -= 0.01f;
        }
        else {
          tempf += 0.01f;
        }

        if (tempf >= softmin && tempf <= softmax) {
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
  int mx, my /*, click = 0 */;
  int retval = WM_UI_HANDLER_CONTINUE;
  bool horizontal = (BLI_rctf_size_x(&but->rect) > BLI_rctf_size_y(&but->rect));

  mx = event->x;
  my = event->y;
  ui_window_to_block(data->region, block, &mx, &my);

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
      /* UNUSED - otherwise code is ok, add back if needed */
#if 0
      else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
        click = 1;
      }
#endif
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY) {
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
      const bool is_motion = (event->type == MOUSEMOVE);
      if (ui_numedit_but_SLI(
              but, data, (horizontal) ? mx : my, horizontal, is_motion, false, false)) {
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
  int mx, my;
  int retval = WM_UI_HANDLER_CONTINUE;
  const bool horizontal = (BLI_rctf_size_x(&but->rect) < BLI_rctf_size_y(&but->rect));

  /* Note: Having to store org point in window space and recompute it to block "space" each time
   *       is not ideal, but this is a way to hack around behavior of ui_window_to_block(), which
   *       returns different results when the block is inside a panel or not...
   *       See T37739.
   */

  mx = event->x;
  my = event->y;
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->val == KM_PRESS) {
      if (event->type == LEFTMOUSE) {
        data->dragstartx = event->x;
        data->dragstarty = event->y;
        button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
        retval = WM_UI_HANDLER_BREAK;
      }
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY) {
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
    if ((ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS &&
         event->ctrl) ||
        (event->type == LEFTMOUSE && event->val == KM_DBL_CLICK)) {
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

    /* first handle click on icondrag type button */
    if (event->type == LEFTMOUSE && but->dragpoin && event->val == KM_PRESS) {
      if (ui_but_contains_point_px_icon(but, data->region, event)) {
        button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
        data->dragstartx = event->x;
        data->dragstarty = event->y;
        return WM_UI_HANDLER_BREAK;
      }
    }
#ifdef USE_DRAG_TOGGLE
    if (event->type == LEFTMOUSE && event->val == KM_PRESS && (ui_but_is_drag_toggle(but))) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->x;
      data->dragstarty = event->y;
      return WM_UI_HANDLER_BREAK;
    }
#endif
    /* regular open menu */
    if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
      button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
      return WM_UI_HANDLER_BREAK;
    }
    else if (but->type == UI_BTYPE_MENU) {
      if (ELEM(event->type, WHEELDOWNMOUSE, WHEELUPMOUSE) && event->ctrl) {
        const int direction = (event->type == WHEELDOWNMOUSE) ? -1 : 1;

        data->value = ui_but_menu_step(but, direction);

        button_activate_state(C, but, BUTTON_STATE_EXIT);
        ui_apply_but(C, but->block, but, data, true);

        /* Button's state need to be changed to EXIT so moving mouse away from this mouse wouldn't
         * lead to cancel changes made to this button, but changing state to EXIT also makes no
         * button active for a while which leads to triggering operator
         * when doing fast scrolling mouse wheel.
         * using post activate stuff from button allows to make button be active again after
         * checking for all all that mouse leave and cancel stuff,
         * so quick scroll wouldn't be an issue anymore.
         * Same goes for scrolling wheel in another direction below (sergey).
         */
        data->postbut = but;
        data->posttype = BUTTON_ACTIVATE_OVER;

        /* without this, a new interface that draws as result of the menu change
         * won't register that the mouse is over it, eg:
         * Alt+MouseWheel over the render slots, without this,
         * the slot menu fails to switch a second time.
         *
         * The active state of the button could be maintained some other way
         * and remove this mousemove event.
         */
        WM_event_add_mousemove(C);

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
  float dx, dy, rad, radsq, mrad, *fp;
  int mdx, mdy;
  bool changed = true;

  /* button is presumed square */
  /* if mouse moves outside of sphere, it does negative normal */

  /* note that both data->vec and data->origvec should be normalized
   * else we'll get a harmless but annoying jump when first clicking */

  fp = data->origvec;
  rad = BLI_rctf_size_x(&but->rect);
  radsq = rad * rad;

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

  dx = (float)(mx + mdx - data->dragstartx);
  dy = (float)(my + mdy - data->dragstarty);

  fp = data->vec;
  mrad = dx * dx + dy * dy;
  if (mrad < radsq) { /* inner circle */
    fp[0] = dx;
    fp[1] = dy;
    fp[2] = sqrtf(radsq - dx * dx - dy * dy);
  }
  else { /* outer circle */

    mrad = rad / sqrtf(mrad);  // veclen

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
    int i;

    /* round each axis of 'fp' to the next increment
     * do this in "angle" space - this gives increments of same size */
    for (i = 0; i < 3; i++) {
      angle = asinf(fp[i]);
      angle_snap = roundf((angle / snap_steps_angle)) * snap_steps_angle;
      fp[i] = sinf(angle_snap);
    }
    normalize_v3(fp);
    changed = !compare_v3v3(fp, data->origvec, FLT_EPSILON);
  }

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static void ui_palette_set_active(uiBut *but)
{
  if ((int)(but->a1) == UI_PALETTE_COLOR) {
    Palette *palette = but->rnapoin.id.data;
    PaletteColor *color = but->rnapoin.data;
    palette->active_color = BLI_findindex(&palette->colors, color);
  }
}

static int ui_do_but_COLOR(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    /* first handle click on icondrag type button */
    if (event->type == LEFTMOUSE && but->dragpoin && event->val == KM_PRESS) {
      ui_palette_set_active(but);
      if (ui_but_contains_point_px_icon(but, data->region, event)) {
        button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
        data->dragstartx = event->x;
        data->dragstarty = event->y;
        return WM_UI_HANDLER_BREAK;
      }
    }
#ifdef USE_DRAG_TOGGLE
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      ui_palette_set_active(but);
      button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
      data->dragstartx = event->x;
      data->dragstarty = event->y;
      return WM_UI_HANDLER_BREAK;
    }
#endif
    /* regular open menu */
    if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
      ui_palette_set_active(but);
      button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
      return WM_UI_HANDLER_BREAK;
    }
    else if (ELEM(event->type, MOUSEPAN, WHEELDOWNMOUSE, WHEELUPMOUSE) && event->ctrl) {
      ColorPicker *cpicker = but->custom_data;
      float hsv_static[3] = {0.0f};
      float *hsv = cpicker ? cpicker->color_data : hsv_static;
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
        float fac = 0.005 * (event->y - event->prevy);
        hsv[2] = clamp_f(hsv[2] + fac, 0.0f, 1.0f);
      }

      hsv_to_rgb_v(hsv, data->vec);
      ui_but_v3_set(but, data->vec);

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      ui_apply_but(C, but->block, but, data, true);
      return WM_UI_HANDLER_BREAK;
    }
    else if ((int)(but->a1) == UI_PALETTE_COLOR && event->type == DELKEY &&
             event->val == KM_PRESS) {
      Palette *palette = but->rnapoin.id.data;
      PaletteColor *color = but->rnapoin.data;

      BKE_palette_color_remove(palette, color);

      button_activate_state(C, but, BUTTON_STATE_EXIT);

      /* this is risky. it works OK for now,
       * but if it gives trouble we should delay execution */
      but->rnapoin = PointerRNA_NULL;
      but->rnaprop = NULL;

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
      if ((int)(but->a1) == UI_PALETTE_COLOR) {
        if (!event->ctrl) {
          float color[3];
          Paint *paint = BKE_paint_get_active_from_context(C);
          Brush *brush = BKE_paint_brush(paint);

          if (brush->flag & BRUSH_USE_GRADIENT) {
            float *target = &brush->gradient->data[brush->gradient->cur].r;

            if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
              RNA_property_float_get_array(&but->rnapoin, but->rnaprop, target);
              IMB_colormanagement_srgb_to_scene_linear_v3(target);
            }
            else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
              RNA_property_float_get_array(&but->rnapoin, but->rnaprop, target);
            }
          }
          else {
            Scene *scene = CTX_data_scene(C);

            if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
              RNA_property_float_get_array(&but->rnapoin, but->rnaprop, color);
              BKE_brush_color_set(scene, brush, color);
            }
            else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
              RNA_property_float_get_array(&but->rnapoin, but->rnaprop, color);
              IMB_colormanagement_scene_linear_to_srgb_v3(color);
              BKE_brush_color_set(scene, brush, color);
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
  int mx, my;

  mx = event->x;
  my = event->y;
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
    else if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
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
    if (v[0] > max) {
      v[0] = max;
    }
    if (v[1] > max) {
      v[1] = max;
    }
    if (v[2] > max) {
      v[2] = max;
    }
  }
}

static void ui_rgb_to_color_picker_HSVCUBE_compat_v(uiBut *but, const float rgb[3], float hsv[3])
{
  if (but->a1 == UI_GRAD_L_ALT) {
    rgb_to_hsl_compat_v(rgb, hsv);
  }
  else {
    rgb_to_hsv_compat_v(rgb, hsv);
  }
}

static void ui_rgb_to_color_picker_HSVCUBE_v(uiBut *but, const float rgb[3], float hsv[3])
{
  if (but->a1 == UI_GRAD_L_ALT) {
    rgb_to_hsl_v(rgb, hsv);
  }
  else {
    rgb_to_hsv_v(rgb, hsv);
  }
}

static void ui_color_picker_to_rgb_HSVCUBE_v(uiBut *but, const float hsv[3], float rgb[3])
{
  if (but->a1 == UI_GRAD_L_ALT) {
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
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;
  float rgb[3];
  float x, y;
  float mx_fl, my_fl;
  bool changed = true;

  ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);

#ifdef USE_CONT_MOUSE_CORRECT
  if (ui_but_is_cursor_warp(but)) {
    /* OK but can go outside bounds */
    data->ungrab_mval[0] = mx_fl;
    data->ungrab_mval[1] = my_fl;
    BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
  }
#endif

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_color_picker_space(but, rgb);

  ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsv);

  /* only apply the delta motion, not absolute */
  if (shift) {
    rcti rect_i;
    float xpos, ypos, hsvo[3];

    BLI_rcti_rctf_copy(&rect_i, &but->rect);

    /* calculate original hsv again */
    copy_v3_v3(rgb, data->origvec);
    ui_scene_linear_to_color_picker_space(but, rgb);

    copy_v3_v3(hsvo, hsv);

    ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsvo);

    /* and original position */
    ui_hsvcube_pos_from_vals(but, &rect_i, hsvo, &xpos, &ypos);

    mx_fl = xpos - (data->dragstartx - mx_fl);
    my_fl = ypos - (data->dragstarty - my_fl);
  }

  /* relative position within box */
  x = ((float)mx_fl - but->rect.xmin) / BLI_rctf_size_x(&but->rect);
  y = ((float)my_fl - but->rect.ymin) / BLI_rctf_size_y(&but->rect);
  CLAMP(x, 0.0f, 1.0f);
  CLAMP(y, 0.0f, 1.0f);

  switch ((int)but->a1) {
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
      float min = but->softmin, max = but->softmax;
      /* exception only for value strip - use the range set in but->min/max */
      hsv[2] = y * (max - min) + min;
      break;
    }
    default:
      BLI_assert(0);
      break;
  }

  if (snap != SNAP_OFF) {
    if (ELEM((int)but->a1, UI_GRAD_HV, UI_GRAD_HS, UI_GRAD_H)) {
      ui_color_snap_hue(snap, &hsv[0]);
    }
  }

  ui_color_picker_to_rgb_HSVCUBE_v(but, hsv, rgb);
  ui_color_picker_to_scene_linear_space(but, rgb);

  /* clamp because with color conversion we can exceed range [#34295] */
  if (but->a1 == UI_GRAD_V_ALT) {
    clamp_axis_max_v3(rgb, but->softmax);
  }

  copy_v3_v3(data->vec, rgb);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

#ifdef WITH_INPUT_NDOF
static void ui_ndofedit_but_HSVCUBE(uiBut *but,
                                    uiHandleButtonData *data,
                                    const wmNDOFMotionData *ndof,
                                    const enum eSnapType snap,
                                    const bool shift)
{
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;
  const float hsv_v_max = max_ff(hsv[2], but->softmax);
  float rgb[3];
  float sensitivity = (shift ? 0.15f : 0.3f) * ndof->dt;

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_color_picker_space(but, rgb);
  ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsv);

  switch ((int)but->a1) {
    case UI_GRAD_SV:
      hsv[1] += ndof->rvec[2] * sensitivity;
      hsv[2] += ndof->rvec[0] * sensitivity;
      break;
    case UI_GRAD_HV:
      hsv[0] += ndof->rvec[2] * sensitivity;
      hsv[2] += ndof->rvec[0] * sensitivity;
      break;
    case UI_GRAD_HS:
      hsv[0] += ndof->rvec[2] * sensitivity;
      hsv[1] += ndof->rvec[0] * sensitivity;
      break;
    case UI_GRAD_H:
      hsv[0] += ndof->rvec[2] * sensitivity;
      break;
    case UI_GRAD_S:
      hsv[1] += ndof->rvec[2] * sensitivity;
      break;
    case UI_GRAD_V:
      hsv[2] += ndof->rvec[2] * sensitivity;
      break;
    case UI_GRAD_V_ALT:
    case UI_GRAD_L_ALT:
      /* vertical 'value' strip */

      /* exception only for value strip - use the range set in but->min/max */
      hsv[2] += ndof->rvec[0] * sensitivity;

      CLAMP(hsv[2], but->softmin, but->softmax);
      break;
    default:
      assert(!"invalid hsv type");
      break;
  }

  if (snap != SNAP_OFF) {
    if (ELEM((int)but->a1, UI_GRAD_HV, UI_GRAD_HS, UI_GRAD_H)) {
      ui_color_snap_hue(snap, &hsv[0]);
    }
  }

  /* ndof specific: the changes above aren't clamping */
  hsv_clamp_v(hsv, hsv_v_max);

  ui_color_picker_to_rgb_HSVCUBE_v(but, hsv, rgb);
  ui_color_picker_to_scene_linear_space(but, rgb);

  copy_v3_v3(data->vec, rgb);
  ui_but_v3_set(but, data->vec);
}
#endif /* WITH_INPUT_NDOF */

static int ui_do_but_HSVCUBE(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx, my;

  mx = event->x;
  my = event->y;
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
      if (ui_numedit_but_HSVCUBE(but, data, mx, my, snap, event->shift != 0)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
#ifdef WITH_INPUT_NDOF
    else if (event->type == NDOF_MOTION) {
      const wmNDOFMotionData *ndof = event->customdata;
      const enum eSnapType snap = ui_event_to_snap(event);

      ui_ndofedit_but_HSVCUBE(but, data, ndof, snap, event->shift != 0);

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      ui_apply_but(C, but->block, but, data, true);

      return WM_UI_HANDLER_BREAK;
    }
#endif /* WITH_INPUT_NDOF */
    /* XXX hardcoded keymap check.... */
    else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
      if (ELEM(but->a1, UI_GRAD_V_ALT, UI_GRAD_L_ALT)) {
        int len;

        /* reset only value */

        len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
        if (ELEM(len, 3, 4)) {
          float rgb[3], def_hsv[3];
          float def[4];
          ColorPicker *cpicker = but->custom_data;
          float *hsv = cpicker->color_data;

          RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
          ui_rgb_to_color_picker_HSVCUBE_v(but, def, def_hsv);

          ui_but_v3_get(but, rgb);
          ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsv);

          def_hsv[0] = hsv[0];
          def_hsv[1] = hsv[1];

          ui_color_picker_to_rgb_HSVCUBE_v(but, def_hsv, rgb);
          ui_but_v3_set(but, rgb);

          RNA_property_update(C, &but->rnapoin, but->rnaprop);
          return WM_UI_HANDLER_BREAK;
        }
      }
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if ((event->type == MOUSEMOVE) || ui_event_is_snap(event)) {
      if (mx != data->draglastx || my != data->draglasty || event->type != MOUSEMOVE) {
        const enum eSnapType snap = ui_event_to_snap(event);

        if (ui_numedit_but_HSVCUBE(but, data, mx, my, snap, event->shift != 0)) {
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
  rcti rect;
  bool changed = true;
  float mx_fl, my_fl;
  float rgb[3];
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;

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

  BLI_rcti_rctf_copy(&rect, &but->rect);

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_color_picker_space(but, rgb);
  ui_rgb_to_color_picker_compat_v(rgb, hsv);

  /* exception, when using color wheel in 'locked' value state:
   * allow choosing a hue for black values, by giving a tiny increment */
  if (cpicker->use_color_lock) {
    if (U.color_picker_type == USER_CP_CIRCLE_HSV) {  // lock
      if (hsv[2] == 0.f) {
        hsv[2] = 0.0001f;
      }
    }
    else {
      if (hsv[2] == 0.0f) {
        hsv[2] = 0.0001f;
      }
      if (hsv[2] >= 0.9999f) {
        hsv[2] = 0.9999f;
      }
    }
  }

  /* only apply the delta motion, not absolute */
  if (shift) {
    float xpos, ypos, hsvo[3], rgbo[3];

    /* calculate original hsv again */
    copy_v3_v3(hsvo, hsv);
    copy_v3_v3(rgbo, data->origvec);
    ui_scene_linear_to_color_picker_space(but, rgbo);
    ui_rgb_to_color_picker_compat_v(rgbo, hsvo);

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

  ui_color_picker_to_rgb_v(hsv, rgb);

  if ((cpicker->use_luminosity_lock)) {
    if (!is_zero_v3(rgb)) {
      normalize_v3_length(rgb, cpicker->luminosity_lock_value);
    }
  }

  ui_color_picker_to_scene_linear_space(but, rgb);
  ui_but_v3_set(but, rgb);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

#ifdef WITH_INPUT_NDOF
static void ui_ndofedit_but_HSVCIRCLE(uiBut *but,
                                      uiHandleButtonData *data,
                                      const wmNDOFMotionData *ndof,
                                      const enum eSnapType snap,
                                      const bool shift)
{
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;
  float rgb[3];
  float phi, r /*, sqr */ /* UNUSED */, v[2];
  float sensitivity = (shift ? 0.06f : 0.3f) * ndof->dt;

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_color_picker_space(but, rgb);
  ui_rgb_to_color_picker_compat_v(rgb, hsv);

  /* Convert current color on hue/sat disc to circular coordinates phi, r */
  phi = fmodf(hsv[0] + 0.25f, 1.0f) * -2.0f * (float)M_PI;
  r = hsv[1];
  /* sqr = r > 0.0f ? sqrtf(r) : 1; */ /* UNUSED */

  /* Convert to 2d vectors */
  v[0] = r * cosf(phi);
  v[1] = r * sinf(phi);

  /* Use ndof device y and x rotation to move the vector in 2d space */
  v[0] += ndof->rvec[2] * sensitivity;
  v[1] += ndof->rvec[0] * sensitivity;

  /* convert back to polar coords on circle */
  phi = atan2f(v[0], v[1]) / (2.0f * (float)M_PI) + 0.5f;

  /* use ndof Y rotation to additionally rotate hue */
  phi += ndof->rvec[1] * sensitivity * 0.5f;
  r = len_v2(v);

  /* convert back to hsv values, in range [0,1] */
  hsv[0] = phi;
  hsv[1] = r;

  /* exception, when using color wheel in 'locked' value state:
   * allow choosing a hue for black values, by giving a tiny increment */
  if (cpicker->use_color_lock) {
    if (U.color_picker_type == USER_CP_CIRCLE_HSV) {  // lock
      if (hsv[2] == 0.f) {
        hsv[2] = 0.0001f;
      }
    }
    else {
      if (hsv[2] == 0.f) {
        hsv[2] = 0.0001f;
      }
      if (hsv[2] == 1.f) {
        hsv[2] = 0.9999f;
      }
    }
  }

  if (snap != SNAP_OFF) {
    ui_color_snap_hue(snap, &hsv[0]);
  }

  hsv_clamp_v(hsv, FLT_MAX);

  ui_color_picker_to_rgb_v(hsv, data->vec);

  if (cpicker->use_luminosity_lock) {
    if (!is_zero_v3(data->vec)) {
      normalize_v3_length(data->vec, cpicker->luminosity_lock_value);
    }
  }

  ui_color_picker_to_scene_linear_space(but, data->vec);
  ui_but_v3_set(but, data->vec);
}
#endif /* WITH_INPUT_NDOF */

static int ui_do_but_HSVCIRCLE(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;
  int mx, my;
  mx = event->x;
  my = event->y;
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
      if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, snap, event->shift != 0)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
#ifdef WITH_INPUT_NDOF
    else if (event->type == NDOF_MOTION) {
      const enum eSnapType snap = ui_event_to_snap(event);
      const wmNDOFMotionData *ndof = event->customdata;

      ui_ndofedit_but_HSVCIRCLE(but, data, ndof, snap, event->shift != 0);

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      ui_apply_but(C, but->block, but, data, true);

      return WM_UI_HANDLER_BREAK;
    }
#endif /* WITH_INPUT_NDOF */
    /* XXX hardcoded keymap check.... */
    else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
      int len;

      /* reset only saturation */

      len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
      if (len >= 3) {
        float rgb[3], def_hsv[3];
        float *def;
        def = MEM_callocN(sizeof(float) * len, "reset_defaults - float");

        RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
        ui_color_picker_to_rgb_v(def, def_hsv);

        ui_but_v3_get(but, rgb);
        ui_rgb_to_color_picker_compat_v(rgb, hsv);

        def_hsv[0] = hsv[0];
        def_hsv[2] = hsv[2];

        hsv_to_rgb_v(def_hsv, rgb);
        ui_but_v3_set(but, rgb);

        RNA_property_update(C, &but->rnapoin, but->rnaprop);

        MEM_freeN(def);
      }
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
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

        if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, snap, event->shift != 0)) {
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
  float dx;
  bool changed = false;

  if (data->draglastx == mx) {
    return changed;
  }

  if (data->coba->tot == 0) {
    return changed;
  }

  dx = ((float)(mx - data->draglastx)) / BLI_rctf_size_x(&but->rect);
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
  ColorBand *coba;
  CBData *cbd;
  /* ignore zoom-level for mindist */
  int mindist = (50 * UI_DPI_FAC) * block->aspect;
  int mx, my, a, xco;

  mx = event->x;
  my = event->y;
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      coba = (ColorBand *)but->poin;

      if (event->ctrl) {
        /* insert new key on mouse location */
        float pos = ((float)(mx - but->rect.xmin)) / BLI_rctf_size_x(&but->rect);
        BKE_colorband_element_add(coba, pos);
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
      else {
        data->dragstartx = mx;
        data->dragstarty = my;
        data->draglastx = mx;
        data->draglasty = my;

        /* activate new key when mouse is close */
        for (a = 0, cbd = coba->data; a < coba->tot; a++, cbd++) {
          xco = but->rect.xmin + (cbd->pos * BLI_rctf_size_x(&but->rect));
          xco = ABS(xco - mx);
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
    else if (ELEM(event->type, ESCKEY, RIGHTMOUSE)) {
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
  float fx, fy, zoomx, zoomy;
  int mx, my, dragx, dragy;
  int a;
  bool changed = false;

  /* evtx evty and drag coords are absolute mousecoords,
   * prevents errors when editing when layout changes */
  mx = evtx;
  my = evty;
  ui_window_to_block(data->region, block, &mx, &my);
  dragx = data->draglastx;
  dragy = data->draglasty;
  ui_window_to_block(data->region, block, &dragx, &dragy);

  zoomx = BLI_rctf_size_x(&but->rect) / BLI_rctf_size_x(&cumap->curr);
  zoomy = BLI_rctf_size_y(&but->rect) / BLI_rctf_size_y(&cumap->curr);

  if (snap) {
    float d[2];

    d[0] = mx - data->dragstartx;
    d[1] = my - data->dragstarty;

    if (len_squared_v2(d) < (3.0f * 3.0f)) {
      snap = false;
    }
  }

  if (data->dragsel != -1) {
    CurveMapPoint *cmp_last = NULL;
    const float mval_factor = ui_mouse_scale_warp_factor(shift);
    bool moved_point = false; /* for ctrl grid, can't use orig coords because of sorting */

    fx = (mx - dragx) / zoomx;
    fy = (my - dragy) / zoomy;

    fx *= mval_factor;
    fy *= mval_factor;

    for (a = 0; a < cuma->totpoint; a++) {
      if (cmp[a].flag & CUMA_SELECT) {
        float origx = cmp[a].x, origy = cmp[a].y;
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

    curvemapping_changed(cumap, false);

    if (moved_point) {
      data->draglastx = evtx;
      data->draglasty = evty;
      changed = true;

#ifdef USE_CONT_MOUSE_CORRECT
      /* note: using 'cmp_last' is weak since there may be multiple points selected,
       * but in practice this isnt really an issue */
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
    fx = (mx - dragx) / zoomx;
    fy = (my - dragy) / zoomy;

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
  int mx, my, a;
  bool changed = false;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  mx = event->x;
  my = event->y;
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      CurveMapping *cumap = (CurveMapping *)but->poin;
      CurveMap *cuma = cumap->cm + cumap->cur;
      CurveMapPoint *cmp;
      const float m_xy[2] = {mx, my};
      float dist_min_sq = SQUARE(U.dpi_fac * 14.0f); /* 14 pixels radius */
      int sel = -1;

      if (event->ctrl) {
        float f_xy[2];
        BLI_rctf_transform_pt_v(&cumap->curr, &but->rect, f_xy, m_xy);

        curvemap_insert(cuma, f_xy[0], f_xy[1]);
        curvemapping_changed(cumap, false);
        changed = true;
      }

      /* check for selecting of a point */
      cmp = cuma->curve; /* ctrl adds point, new malloc */
      for (a = 0; a < cuma->totpoint; a++) {
        float f_xy[2];
        BLI_rctf_transform_pt_v(&but->rect, &cumap->curr, f_xy, &cmp[a].x);
        const float dist_sq = len_squared_v2v2(m_xy, f_xy);
        if (dist_sq < dist_min_sq) {
          sel = a;
          dist_min_sq = dist_sq;
        }
      }

      if (sel == -1) {
        int i;
        float f_xy[2], f_xy_prev[2];

        /* if the click didn't select anything, check if it's clicked on the
         * curve itself, and if so, add a point */
        cmp = cuma->table;

        BLI_rctf_transform_pt_v(&but->rect, &cumap->curr, f_xy, &cmp[0].x);

        /* with 160px height 8px should translate to the old 0.05 coefficient at no zoom */
        dist_min_sq = SQUARE(U.dpi_fac * 8.0f);

        /* loop through the curve segment table and find what's near the mouse. */
        for (i = 1; i <= CM_TABLE; i++) {
          copy_v2_v2(f_xy_prev, f_xy);
          BLI_rctf_transform_pt_v(&but->rect, &cumap->curr, f_xy, &cmp[i].x);

          if (dist_squared_to_line_segment_v2(m_xy, f_xy_prev, f_xy) < dist_min_sq) {
            BLI_rctf_transform_pt_v(&cumap->curr, &but->rect, f_xy, m_xy);

            curvemap_insert(cuma, f_xy[0], f_xy[1]);
            curvemapping_changed(cumap, false);

            changed = true;

            /* reset cmp back to the curve points again,
             * rather than drawing segments */
            cmp = cuma->curve;

            /* find newly added point and make it 'sel' */
            for (a = 0; a < cuma->totpoint; a++) {
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
        if (!event->shift) {
          for (a = 0; a < cuma->totpoint; a++) {
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

      data->dragstartx = event->x;
      data->dragstarty = event->y;
      data->draglastx = event->x;
      data->draglasty = event->y;

      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == MOUSEMOVE) {
      if (event->x != data->draglastx || event->y != data->draglasty) {

        if (ui_numedit_but_CURVE(
                block, but, data, event->x, event->y, event->ctrl != 0, event->shift != 0)) {
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
          if (!event->shift) {
            for (a = 0; a < cuma->totpoint; a++) {
              cmp[a].flag &= ~CUMA_SELECT;
            }
            cmp[data->dragsel].flag |= CUMA_SELECT;
          }
        }
        else {
          curvemapping_changed(cumap, true); /* remove doubles */
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

static bool ui_numedit_but_HISTOGRAM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
  Histogram *hist = (Histogram *)but->poin;
  bool changed = true;
  float dy = my - data->draglasty;

  /* scale histogram values (dy / 10 for better control) */
  const float yfac = min_ff(pow2f(hist->ymax), 1.0f) * 0.5f;
  hist->ymax += (dy * 0.1f) * yfac;

  /* 0.1 allows us to see HDR colors up to 10 */
  CLAMP(hist->ymax, 0.1f, 100.f);

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static int ui_do_but_HISTOGRAM(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx, my;

  mx = event->x;
  my = event->y;
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
    else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
      Histogram *hist = (Histogram *)but->poin;
      hist->ymax = 1.f;

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY) {
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
  bool changed = true;
  float dy;

  dy = my - data->draglasty;

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
  int mx, my;

  mx = event->x;
  my = event->y;
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
    else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
      Scopes *scopes = (Scopes *)but->poin;
      scopes->wavefrm_yfac = 1.f;

      button_activate_state(C, but, BUTTON_STATE_EXIT);
      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY) {
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
  bool changed = true;
  float dx, dy;

  dx = mx - data->draglastx;
  dy = my - data->draglasty;

  if (shift) {
    dx /= 5.0f;
    dy /= 5.0f;
  }

  if (!scopes->track_locked) {
    if (scopes->marker->framenr != scopes->framenr) {
      scopes->marker = BKE_tracking_marker_ensure(scopes->track, scopes->framenr);
    }

    scopes->marker->flag &= ~(MARKER_DISABLED | MARKER_TRACKED);
    scopes->marker->pos[0] += -dx * scopes->slide_scale[0] / BLI_rctf_size_x(&but->block->rect);
    scopes->marker->pos[1] += -dy * scopes->slide_scale[1] / BLI_rctf_size_y(&but->block->rect);

    WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);
  }

  scopes->ok = 0;

  data->draglastx = mx;
  data->draglasty = my;

  return changed;
}

static int ui_do_but_TRACKPREVIEW(
    bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
  int mx, my;

  mx = event->x;
  my = event->y;
  ui_window_to_block(data->region, block, &mx, &my);

  if (data->state == BUTTON_STATE_HIGHLIGHT) {
    if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
      data->dragstartx = mx;
      data->dragstarty = my;
      data->draglastx = mx;
      data->draglasty = my;
      button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

      /* also do drag the first time */
      if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->shift != 0)) {
        ui_numedit_apply(C, block, but, data);
      }

      return WM_UI_HANDLER_BREAK;
    }
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    if (event->type == ESCKEY) {
      if (event->val == KM_PRESS) {
        data->cancel = true;
        data->escapecancel = true;
        button_activate_state(C, but, BUTTON_STATE_EXIT);
      }
    }
    else if (event->type == MOUSEMOVE) {
      if (mx != data->draglastx || my != data->draglasty) {
        if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->shift != 0)) {
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
  uiHandleButtonData *data;
  int retval;

  data = but->active;
  retval = WM_UI_HANDLER_CONTINUE;

  bool is_disabled = but->flag & UI_BUT_DISABLED;

  /* if but->pointype is set, but->poin should be too */
  BLI_assert(!but->pointype || but->poin);

  /* Only hard-coded stuff here, button interactions with configurable
   * keymaps are handled using operators (see #ED_keymap_ui). */

  if ((data->state == BUTTON_STATE_HIGHLIGHT) || (event->type == EVT_DROP)) {

    /* handle copy and paste */
    bool is_press_ctrl_but_no_shift = event->val == KM_PRESS && IS_EVENT_MOD(event, ctrl, oskey) &&
                                      !event->shift;
    bool do_copy = event->type == CKEY && is_press_ctrl_but_no_shift;
    bool do_paste = event->type == VKEY && is_press_ctrl_but_no_shift;

    /* Specific handling for listrows, we try to find their overlapping tex button. */
    if ((do_copy || do_paste) && but->type == UI_BTYPE_LISTROW) {
      uiBut *labelbut = ui_but_list_row_text_activate(C, but, data, event, BUTTON_ACTIVATE_OVER);
      if (labelbut) {
        but = labelbut;
        data = but->active;
      }
    }

    /* do copy first, because it is the only allowed operator when disabled */
    if (do_copy) {
      ui_but_copy(C, but, event->alt);
      return WM_UI_HANDLER_BREAK;
    }

    /* handle menu */
    if ((event->type == RIGHTMOUSE) && !IS_EVENT_MOD(event, shift, ctrl, alt, oskey) &&
        (event->val == KM_PRESS)) {
      /* RMB has two options now */
      if (ui_popup_context_menu_for_button(C, but)) {
        return WM_UI_HANDLER_BREAK;
      }
    }

    if (is_disabled) {
      return WM_UI_HANDLER_CONTINUE;
    }

    if (do_paste) {
      ui_but_paste(C, but, data, event->alt);
      return WM_UI_HANDLER_BREAK;
    }

    /* handle drop */
    if (event->type == EVT_DROP) {
      ui_but_drop(C, event, but, data);
    }
  }

  if (but->flag & UI_BUT_DISABLED) {
    return WM_UI_HANDLER_CONTINUE;
  }

  switch (but->type) {
    case UI_BTYPE_BUT:
      retval = ui_do_but_BUT(C, but, data, event);
      break;
    case UI_BTYPE_KEY_EVENT:
      retval = ui_do_but_KEYEVT(C, but, data, event);
      break;
    case UI_BTYPE_HOTKEY_EVENT:
      retval = ui_do_but_HOTKEYEVT(C, but, data, event);
      break;
    case UI_BTYPE_TAB:
      retval = ui_do_but_TAB(C, block, but, data, event);
      break;
    case UI_BTYPE_BUT_TOGGLE:
    case UI_BTYPE_TOGGLE:
    case UI_BTYPE_ICON_TOGGLE:
    case UI_BTYPE_ICON_TOGGLE_N:
    case UI_BTYPE_TOGGLE_N:
    case UI_BTYPE_CHECKBOX:
    case UI_BTYPE_CHECKBOX_N:
    case UI_BTYPE_ROW:
      retval = ui_do_but_TOG(C, but, data, event);
      break;
    case UI_BTYPE_SCROLL:
      retval = ui_do_but_SCROLL(C, block, but, data, event);
      break;
    case UI_BTYPE_GRIP:
      retval = ui_do_but_GRIP(C, block, but, data, event);
      break;
    case UI_BTYPE_NUM:
      retval = ui_do_but_NUM(C, block, but, data, event);
      break;
    case UI_BTYPE_NUM_SLIDER:
      retval = ui_do_but_SLI(C, block, but, data, event);
      break;
    case UI_BTYPE_LISTBOX:
      /* Nothing to do! */
      break;
    case UI_BTYPE_LISTROW:
      retval = ui_do_but_LISTROW(C, but, data, event);
      break;
    case UI_BTYPE_ROUNDBOX:
    case UI_BTYPE_LABEL:
    case UI_BTYPE_IMAGE:
    case UI_BTYPE_PROGRESS_BAR:
    case UI_BTYPE_NODE_SOCKET:
      retval = ui_do_but_EXIT(C, but, data, event);
      break;
    case UI_BTYPE_HISTOGRAM:
      retval = ui_do_but_HISTOGRAM(C, block, but, data, event);
      break;
    case UI_BTYPE_WAVEFORM:
      retval = ui_do_but_WAVEFORM(C, block, but, data, event);
      break;
    case UI_BTYPE_VECTORSCOPE:
      /* Nothing to do! */
      break;
    case UI_BTYPE_TEXT:
    case UI_BTYPE_SEARCH_MENU:
      if ((but->type == UI_BTYPE_SEARCH_MENU) && (but->flag & UI_BUT_VALUE_CLEAR)) {
        retval = ui_do_but_SEARCH_UNLINK(C, block, but, data, event);
        if (retval & WM_UI_HANDLER_BREAK) {
          break;
        }
      }
      retval = ui_do_but_TEX(C, block, but, data, event);
      break;
    case UI_BTYPE_MENU:
    case UI_BTYPE_POPOVER:
    case UI_BTYPE_BLOCK:
    case UI_BTYPE_PULLDOWN:
      retval = ui_do_but_BLOCK(C, but, data, event);
      break;
    case UI_BTYPE_BUT_MENU:
      retval = ui_do_but_BUT(C, but, data, event);
      break;
    case UI_BTYPE_COLOR:
      if (but->a1 == -1) {
        /* signal to prevent calling up color picker */
        retval = ui_do_but_EXIT(C, but, data, event);
      }
      else {
        retval = ui_do_but_COLOR(C, but, data, event);
      }
      break;
    case UI_BTYPE_UNITVEC:
      retval = ui_do_but_UNITVEC(C, block, but, data, event);
      break;
    case UI_BTYPE_COLORBAND:
      retval = ui_do_but_COLORBAND(C, block, but, data, event);
      break;
    case UI_BTYPE_CURVE:
      retval = ui_do_but_CURVE(C, block, but, data, event);
      break;
    case UI_BTYPE_HSVCUBE:
      retval = ui_do_but_HSVCUBE(C, block, but, data, event);
      break;
    case UI_BTYPE_HSVCIRCLE:
      retval = ui_do_but_HSVCIRCLE(C, block, but, data, event);
      break;
    case UI_BTYPE_TRACK_PREVIEW:
      retval = ui_do_but_TRACKPREVIEW(C, block, but, data, event);
      break;

      /* quiet warnings for unhandled types */
    case UI_BTYPE_SEPR:
    case UI_BTYPE_SEPR_LINE:
    case UI_BTYPE_SEPR_SPACER:
    case UI_BTYPE_EXTRA:
      break;
  }

  /* reset to default (generic function, only use if not handled by switch above) */
  /* XXX hardcoded keymap check.... */
  data = but->active;
  if (data && data->state == BUTTON_STATE_HIGHLIGHT) {
    if ((retval == WM_UI_HANDLER_CONTINUE) &&
        (event->type == BACKSPACEKEY && event->val == KM_PRESS)) {
      /* ctrl+backspace = reset active button; backspace = reset a whole array*/
      ui_but_default_set(C, !event->ctrl, true);
      ED_region_tag_redraw(data->region);
      retval = WM_UI_HANDLER_BREAK;
    }
  }

#ifdef USE_DRAG_MULTINUM
  if (data) {
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) ||
        /* if we started dragging, progress on any event */
        (data->multi_data.init == BUTTON_MULTI_INIT_SETUP)) {
      if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER) &&
          ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING)) {
        /* initialize! */
        if (data->multi_data.init == BUTTON_MULTI_INIT_UNSET) {
          /* --> (BUTTON_MULTI_INIT_SETUP | BUTTON_MULTI_INIT_DISABLE) */

          const float margin_y = DRAG_MULTINUM_THRESHOLD_DRAG_Y / sqrtf(block->aspect);

          /* check if we have a vertical gesture */
          if (len_squared_v2(data->multi_data.drag_dir) > (margin_y * margin_y)) {
            const float dir_nor_y[2] = {0.0, 1.0f};
            float dir_nor_drag[2];

            normalize_v2_v2(dir_nor_drag, data->multi_data.drag_dir);

            if (fabsf(dot_v2v2(dir_nor_drag, dir_nor_y)) > DRAG_MULTINUM_THRESHOLD_VERTICAL) {
              data->multi_data.init = BUTTON_MULTI_INIT_SETUP;
              data->multi_data.drag_lock_x = event->x;
            }
            else {
              data->multi_data.init = BUTTON_MULTI_INIT_DISABLE;
            }
          }
        }
        else if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
          /* --> (BUTTON_MULTI_INIT_ENABLE) */
          const float margin_x = DRAG_MULTINUM_THRESHOLD_DRAG_X / sqrtf(block->aspect);
          /* check if we're dont setting buttons */
          if ((data->str &&
               ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING)) ||
              ((abs(data->multi_data.drag_lock_x - event->x) > margin_x) &&
               /* just to be sure, check we're dragging more hoz then virt */
               abs(event->prevx - event->x) > abs(event->prevy - event->y))) {
            if (data->multi_data.has_mbuts) {
              ui_multibut_states_create(but, data);
              data->multi_data.init = BUTTON_MULTI_INIT_ENABLE;
            }
            else {
              data->multi_data.init = BUTTON_MULTI_INIT_DISABLE;
            }
          }
        }

        if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
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

static void ui_blocks_set_tooltips(ARegion *ar, const bool enable)
{
  uiBlock *block;

  if (!ar) {
    return;
  }

  /* we disabled buttons when when they were already shown, and
   * re-enable them on mouse move */
  for (block = ar->uiblocks.first; block; block = block->next) {
    block->tooltipdisabled = !enable;
  }
}

/**
 * Recreate tooltip (use to update dynamic tips)
 */
void UI_but_tooltip_refresh(bContext *C, uiBut *but)
{
  uiHandleButtonData *data = but->active;
  if (data) {
    bScreen *sc = WM_window_get_active_screen(data->window);
    if (sc->tool_tip && sc->tool_tip->region) {
      WM_tooltip_refresh(C, data->window);
    }
  }
}

/* removes tooltip timer from active but
 * (meaning tooltip is disabled until it's reenabled again) */
void UI_but_tooltip_timer_remove(bContext *C, uiBut *but)
{
  uiHandleButtonData *data;

  data = but->active;
  if (data) {
    if (data->autoopentimer) {
      WM_event_remove_timer(data->wm, data->window, data->autoopentimer);
      data->autoopentimer = NULL;
    }

    if (data->window) {
      WM_tooltip_clear(C, data->window);
    }
  }
}

static ARegion *ui_but_tooltip_init(
    bContext *C, ARegion *ar, int *pass, double *r_pass_delay, bool *r_exit_on_event)
{
  bool is_label = false;
  if (*pass == 1) {
    is_label = true;
    (*pass)--;
    (*r_pass_delay) = UI_TOOLTIP_DELAY - UI_TOOLTIP_DELAY_LABEL;
  }

  uiBut *but = UI_region_active_but_get(ar);
  *r_exit_on_event = false;
  if (but) {
    return UI_tooltip_create_from_button(C, ar, but, is_label);
  }
  return NULL;
}

static void button_tooltip_timer_reset(bContext *C, uiBut *but)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  uiHandleButtonData *data = but->active;

  WM_tooltip_timer_clear(C, data->window);

  if ((U.flag & USER_TOOLTIPS) || (data->tooltip_force)) {
    if (!but->block->tooltipdisabled) {
      if (!wm->drags.first) {
        bool is_label = UI_but_has_tooltip_label(but);
        double delay = is_label ? UI_TOOLTIP_DELAY_LABEL : UI_TOOLTIP_DELAY;
        WM_tooltip_timer_init_ex(C, data->window, data->region, ui_but_tooltip_init, delay);
        if (is_label) {
          bScreen *sc = WM_window_get_active_screen(data->window);
          if (sc->tool_tip) {
            sc->tool_tip->pass = 1;
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
  uiHandleButtonData *data;

  data = but->active;
  if (data->state == state) {
    return;
  }

  /* highlight has timers for tooltips and auto open */
  if (state == BUTTON_STATE_HIGHLIGHT) {
    but->flag &= ~UI_SELECT;

    button_tooltip_timer_reset(C, but);

    /* automatic open pulldown block timer */
    if (ELEM(but->type, UI_BTYPE_BLOCK, UI_BTYPE_PULLDOWN, UI_BTYPE_POPOVER)) {
      if (data->used_mouse && !data->autoopentimer) {
        int time;

        if (but->block->auto_open == true) { /* test for toolbox */
          time = 1;
        }
        else if ((but->block->flag & UI_BLOCK_LOOP && but->type != UI_BTYPE_BLOCK) ||
                 (but->block->auto_open == true)) {
          time = 5 * U.menuthreshold2;
        }
        else if (U.uiflag & USER_MENUOPENAUTO) {
          time = 5 * U.menuthreshold1;
        }
        else {
          time = -1; /* do nothing */
        }

        if (time >= 0) {
          data->autoopentimer = WM_event_add_timer(
              data->wm, data->window, TIMER, 0.02 * (double)time);
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
      WM_cursor_grab_enable(CTX_wm_window(C), WM_CURSOR_WRAP_XY, true, NULL);
    }
    ui_numedit_begin(but, data);
  }
  else if (data->state == BUTTON_STATE_NUM_EDITING) {
    ui_numedit_end(but, data);

    if (but->flag & UI_BUT_DRIVEN) {
      /* Only warn when editing stepping/dragging the value.
       * No warnings should show for editing driver expressions though!
       */
      if (state != BUTTON_STATE_TEXT_EDITING) {
        WM_report(RPT_INFO,
                  "Can't edit driven number value, see graph editor for the driver setup.");
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
        WM_cursor_grab_disable(data->window, NULL);
      }
#else
      WM_cursor_grab_disable(data->window, NULL);
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
    data->flashtimer = WM_event_add_timer(data->wm, data->window, TIMER, BUTTON_FLASH_DELAY);
  }
  else if (data->flashtimer) {
    WM_event_remove_timer(data->wm, data->window, data->flashtimer);
    data->flashtimer = NULL;
  }

  /* add hold timer if it's used */
  if (state == BUTTON_STATE_WAIT_RELEASE && (but->hold_func != NULL)) {
    data->hold_action_timer = WM_event_add_timer(
        data->wm, data->window, TIMER, BUTTON_AUTO_OPEN_THRESH);
  }
  else if (data->hold_action_timer) {
    WM_event_remove_timer(data->wm, data->window, data->hold_action_timer);
    data->hold_action_timer = NULL;
  }

  /* add a blocking ui handler at the window handler for blocking, modal states
   * but not for popups, because we already have a window level handler*/
  if (!(but->block->handle && but->block->handle->popup)) {
    if (button_modal_state(state)) {
      if (!button_modal_state(data->state)) {
        WM_event_add_ui_handler(
            C, &data->window->modalhandlers, ui_handler_region_menu, NULL, data, 0);
      }
    }
    else {
      if (button_modal_state(data->state)) {
        /* true = postpone free */
        WM_event_remove_ui_handler(
            &data->window->modalhandlers, ui_handler_region_menu, NULL, data, true);
      }
    }
  }

  /* wait for mousemove to enable drag */
  if (state == BUTTON_STATE_WAIT_DRAG) {
    but->flag &= ~UI_SELECT;
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
  ED_region_tag_redraw(data->region);
}

static void button_activate_init(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type)
{
  uiHandleButtonData *data;

  /* setup struct */
  data = MEM_callocN(sizeof(uiHandleButtonData), "uiHandleButtonData");
  data->wm = CTX_wm_manager(C);
  data->window = CTX_wm_window(C);
  data->region = ar;

#ifdef USE_CONT_MOUSE_CORRECT
  copy_v2_fl(data->ungrab_mval, FLT_MAX);
#endif

  if (ELEM(but->type, UI_BTYPE_CURVE, UI_BTYPE_SEARCH_MENU)) {
    /* XXX curve is temp */
  }
  else {
    if ((but->flag & UI_BUT_UPDATE_DELAY) == 0) {
      data->interactive = true;
    }
  }

  data->state = BUTTON_STATE_INIT;

  /* activate button */
  but->flag |= UI_ACTIVE;

  but->active = data;

  /* we disable auto_open in the block after a threshold, because we still
   * want to allow auto opening adjacent menus even if no button is activated
   * in between going over to the other button, but only for a short while */
  if (type == BUTTON_ACTIVATE_OVER && but->block->auto_open == true) {
    if (but->block->auto_open_last + BUTTON_AUTO_OPEN_THRESH < PIL_check_seconds_timer()) {
      but->block->auto_open = false;
    }
  }

  if (type == BUTTON_ACTIVATE_OVER) {
    data->used_mouse = true;
  }
  button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);

  /* activate right away */
  if (but->flag & UI_BUT_IMMEDIATE) {
    if (but->type == UI_BTYPE_HOTKEY_EVENT) {
      button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
    }
    /* .. more to be added here */
  }

  if (type == BUTTON_ACTIVATE_OPEN) {
    button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);

    /* activate first button in submenu */
    if (data->menu && data->menu->region) {
      ARegion *subar = data->menu->region;
      uiBlock *subblock = subar->uiblocks.first;
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

  if (but->type == UI_BTYPE_GRIP) {
    const bool horizontal = (BLI_rctf_size_x(&but->rect) < BLI_rctf_size_y(&but->rect));
    WM_cursor_modal_set(data->window, horizontal ? CURSOR_X_MOVE : CURSOR_Y_MOVE);
  }
  else if (but->type == UI_BTYPE_NUM) {
    ui_numedit_set_active(but);
  }

  if (UI_but_has_tooltip_label(but)) {
    /* Show a label for this button. */
    bScreen *sc = WM_window_get_active_screen(data->window);
    if ((PIL_check_seconds_timer() - WM_tooltip_time_closed()) < 0.1) {
      WM_tooltip_immediate_init(C, CTX_wm_window(C), ar, ui_but_tooltip_init);
      if (sc->tool_tip) {
        sc->tool_tip->pass = 1;
      }
    }
  }
}

static void button_activate_exit(
    bContext *C, uiBut *but, uiHandleButtonData *data, const bool mousemove, const bool onfree)
{
  uiBlock *block = but->block;
  uiBut *bt;

  if (but->type == UI_BTYPE_GRIP) {
    WM_cursor_modal_restore(data->window);
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
    for (bt = block->buttons.first; bt; bt = bt->next) {
      if (bt->flag & UI_BUT_DRAG_MULTI) {
        bt->flag &= ~UI_BUT_DRAG_MULTI;

        if (!data->cancel) {
          ui_apply_but_autokey(C, bt);
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
      for (int i = 0; i < selctx_data->elems_len; i++) {
        uiSelectContextElem *other = &selctx_data->elems[i];
        but_temp.rnapoin = other->ptr;
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

  /* disable tooltips until mousemove + last active flag */
  for (block = data->region->uiblocks.first; block; block = block->next) {
    for (bt = block->buttons.first; bt; bt = bt->next) {
      bt->flag &= ~UI_BUT_LAST_ACTIVE;
    }

    block->tooltipdisabled = 1;
  }

  ui_blocks_set_tooltips(data->region, false);

  /* clean up */
  if (data->str) {
    MEM_freeN(data->str);
  }
  if (data->origstr) {
    MEM_freeN(data->origstr);
  }

#ifdef USE_ALLSELECT
  ui_selectcontext_end(but, &data->select_others);
#endif

  if (data->changed_cursor) {
    WM_cursor_modal_restore(data->window);
  }

  /* redraw and refresh (for popups) */
  ED_region_tag_redraw(data->region);
  ED_region_tag_refresh_ui(data->region);

  /* clean up button */
  if (but->active) {
    MEM_freeN(but->active);
    but->active = NULL;
  }

  but->flag &= ~(UI_ACTIVE | UI_SELECT);
  but->flag |= UI_BUT_LAST_ACTIVE;
  if (!onfree) {
    ui_but_update(but);
  }

  /* adds empty mousemove in queue for re-init handler, in case mouse is
   * still over a button. We cannot just check for this ourselves because
   * at this point the mouse may be over a button in another region */
  if (mousemove) {
    WM_event_add_mousemove(C);
  }
}

void ui_but_active_free(const bContext *C, uiBut *but)
{
  uiHandleButtonData *data;

  /* this gets called when the button somehow disappears while it is still
   * active, this is bad for user interaction, but we need to handle this
   * case cleanly anyway in case it happens */
  if (but->active) {
    data = but->active;
    data->cancel = true;
    button_activate_exit((bContext *)C, but, data, false, true);
  }
}

/* returns the active button with an optional checking function */
static uiBut *ui_context_button_active(ARegion *ar, bool (*but_check_cb)(uiBut *))
{
  uiBut *but_found = NULL;

  while (ar) {
    uiBlock *block;
    uiBut *but, *activebut = NULL;

    /* find active button */
    for (block = ar->uiblocks.first; block; block = block->next) {
      for (but = block->buttons.first; but; but = but->next) {
        if (but->active) {
          activebut = but;
        }
        else if (!activebut && (but->flag & UI_BUT_LAST_ACTIVE)) {
          activebut = but;
        }
      }
    }

    if (activebut && (but_check_cb == NULL || but_check_cb(activebut))) {
      uiHandleButtonData *data = activebut->active;

      but_found = activebut;

      /* recurse into opened menu, like colorpicker case */
      if (data && data->menu && (ar != data->menu->region)) {
        ar = data->menu->region;
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

static bool ui_context_rna_button_active_test(uiBut *but)
{
  return (but->rnapoin.data != NULL);
}
static uiBut *ui_context_rna_button_active(const bContext *C)
{
  return ui_context_button_active(CTX_wm_region(C), ui_context_rna_button_active_test);
}

uiBut *UI_context_active_but_get(const struct bContext *C)
{
  return ui_context_button_active(CTX_wm_region(C), NULL);
}

uiBut *UI_region_active_but_get(ARegion *ar)
{
  return ui_context_button_active(ar, NULL);
}

uiBut *UI_region_but_find_rect_over(const ARegion *ar, const rcti *rect_px)
{
  return ui_but_find_rect_over(ar, rect_px);
}

/**
 * Version of #UI_context_active_but_get that also returns RNA property info.
 * Helper function for insert keyframe, reset to default, etc operators.
 *
 * \return active button, NULL if none found or if it doesn't contain valid RNA data.
 */
uiBut *UI_context_active_but_prop_get(const bContext *C,
                                      struct PointerRNA *r_ptr,
                                      struct PropertyRNA **r_prop,
                                      int *r_index)
{
  uiBut *activebut = ui_context_rna_button_active(C);

  if (activebut && activebut->rnapoin.data) {
    *r_ptr = activebut->rnapoin;
    *r_prop = activebut->rnaprop;
    *r_index = activebut->rnaindex;
  }
  else {
    memset(r_ptr, 0, sizeof(*r_ptr));
    *r_prop = NULL;
    *r_index = 0;
  }

  return activebut;
}

void UI_context_active_but_prop_handle(bContext *C)
{
  uiBut *activebut = ui_context_rna_button_active(C);
  if (activebut) {
    /* TODO, look into a better way to handle the button change
     * currently this is mainly so reset defaults works for the
     * operator redo panel - campbell */
    uiBlock *block = activebut->block;
    if (block->handle_func) {
      block->handle_func(C, block->handle_func_arg, activebut->retval);
    }
  }
}

wmOperator *UI_context_active_operator_get(const struct bContext *C)
{
  ARegion *ar_ctx = CTX_wm_region(C);
  uiBlock *block;

  /* background mode */
  if (ar_ctx == NULL) {
    return NULL;
  }

  /* scan active regions ui */
  for (block = ar_ctx->uiblocks.first; block; block = block->next) {
    if (block->ui_operator) {
      return block->ui_operator;
    }
  }

  /* scan popups */
  {
    bScreen *sc = CTX_wm_screen(C);
    ARegion *ar;

    for (ar = sc->regionbase.first; ar; ar = ar->next) {
      if (ar == ar_ctx) {
        continue;
      }
      for (block = ar->uiblocks.first; block; block = block->next) {
        if (block->ui_operator) {
          return block->ui_operator;
        }
      }
    }
  }

  return NULL;
}

/* helper function for insert keyframe, reset to default, etc operators */
void UI_context_update_anim_flag(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *ar = CTX_wm_region(C);
  uiBlock *block;
  uiBut *but, *activebut;

  while (ar) {
    /* find active button */
    activebut = NULL;

    for (block = ar->uiblocks.first; block; block = block->next) {
      for (but = block->buttons.first; but; but = but->next) {
        ui_but_anim_flag(but, (scene) ? scene->r.cfra : 0.0f);
        ui_but_override_flag(but);
        if (UI_but_is_decorator(but)) {
          ui_but_anim_decorate_update_from_flag(but);
        }

        ED_region_tag_redraw(ar);

        if (but->active) {
          activebut = but;
        }
        else if (!activebut && (but->flag & UI_BUT_LAST_ACTIVE)) {
          activebut = but;
        }
      }
    }

    if (activebut) {
      /* always recurse into opened menu, so all buttons update (like colorpicker) */
      uiHandleButtonData *data = activebut->active;
      if (data && data->menu) {
        ar = data->menu->region;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Activation Handling
 * \{ */

static uiBut *ui_but_find_open_event(ARegion *ar, const wmEvent *event)
{
  uiBlock *block;
  uiBut *but;

  for (block = ar->uiblocks.first; block; block = block->next) {
    for (but = block->buttons.first; but; but = but->next) {
      if (but == event->customdata) {
        return but;
      }
    }
  }
  return NULL;
}

static int ui_handle_button_over(bContext *C, const wmEvent *event, ARegion *ar)
{
  uiBut *but;

  if (event->type == MOUSEMOVE) {
    but = ui_but_find_mouse_over(ar, event);
    if (but) {
      button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);

      if (event->alt && but->active) {
        /* display tooltips if holding alt on mouseover when tooltips are off in prefs */
        but->active->tooltip_force = true;
      }
    }
  }
  else if (event->type == EVT_BUT_OPEN) {
    but = ui_but_find_open_event(ar, event);
    if (but) {
      button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
      ui_do_button(C, but->block, but, event);
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

/* exported to interface.c: UI_but_active_only() */
void ui_but_activate_event(bContext *C, ARegion *ar, uiBut *but)
{
  wmWindow *win = CTX_wm_window(C);
  wmEvent event;

  button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);

  wm_event_init_from_window(win, &event);
  event.type = EVT_BUT_OPEN;
  event.val = KM_PRESS;
  event.customdata = but;
  event.customdatafree = false;

  ui_do_button(C, but->block, but, &event);
}

/**
 * Simulate moving the mouse over a button (or navigating to it with arrow keys).
 *
 * exported so menus can start with a highlighted button,
 * even if the mouse isnt over it
 */
void ui_but_activate_over(bContext *C, ARegion *ar, uiBut *but)
{
  button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
}

void ui_but_execute_begin(struct bContext *UNUSED(C),
                          struct ARegion *ar,
                          uiBut *but,
                          void **active_back)
{
  /* note: ideally we would not have to change 'but->active' however
   * some functions we call don't use data (as they should be doing) */
  uiHandleButtonData *data;
  *active_back = but->active;
  data = MEM_callocN(sizeof(uiHandleButtonData), "uiHandleButtonData_Fake");
  but->active = data;
  data->region = ar;
}

void ui_but_execute_end(struct bContext *C,
                        struct ARegion *UNUSED(ar),
                        uiBut *but,
                        void *active_back)
{
  ui_apply_but(C, but->block, but, but->active, true);

  if ((but->flag & UI_BUT_DRAG_MULTI) == 0) {
    ui_apply_but_autokey(C, but);
  }
  /* use onfree event so undo is handled by caller and apply is already done above */
  button_activate_exit((bContext *)C, but, but->active, false, true);
  but->active = active_back;
}

static void ui_handle_button_activate(bContext *C,
                                      ARegion *ar,
                                      uiBut *but,
                                      uiButtonActivateType type)
{
  uiBut *oldbut;
  uiHandleButtonData *data;

  oldbut = ui_region_find_active_but(ar);
  if (oldbut) {
    data = oldbut->active;
    data->cancel = true;
    button_activate_exit(C, oldbut, data, false, false);
  }

  button_activate_init(C, ar, but, type);
}

/**
 * Use for key accelerator or default key to activate the button even if its not active.
 */
static bool ui_handle_button_activate_by_type(bContext *C, ARegion *ar, uiBut *but)
{
  if (but->type == UI_BTYPE_BUT_MENU) {
    /* mainly for operator buttons */
    ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_APPLY);
  }
  else if (ELEM(but->type, UI_BTYPE_BLOCK, UI_BTYPE_PULLDOWN)) {
    /* open sub-menus (like right arrow key) */
    ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_OPEN);
  }
  else if (but->type == UI_BTYPE_MENU) {
    /* activate menu items */
    ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE);
  }
  else {
#ifdef DEBUG
    printf("%s: error, unhandled type: %u\n", __func__, but->type);
#endif
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handle Events for Activated Buttons
 * \{ */

static bool ui_button_value_default(uiBut *but, double *r_value)
{
  if (but->rnaprop != NULL && ui_but_is_rna_valid(but)) {
    int type = RNA_property_type(but->rnaprop);
    if (ELEM(type, PROP_FLOAT, PROP_INT)) {
      double default_value;
      switch (type) {
        case PROP_INT:
          if (RNA_property_array_check(but->rnaprop)) {
            default_value = (double)RNA_property_int_get_default_index(
                &but->rnapoin, but->rnaprop, but->rnaindex);
          }
          else {
            default_value = (double)RNA_property_int_get_default(&but->rnapoin, but->rnaprop);
          }
          break;
        case PROP_FLOAT:
          if (RNA_property_array_check(but->rnaprop)) {
            default_value = (double)RNA_property_float_get_default_index(
                &but->rnapoin, but->rnaprop, but->rnaindex);
          }
          else {
            default_value = (double)RNA_property_float_get_default(&but->rnapoin, but->rnaprop);
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
  uiBlock *block;
  ARegion *ar;
  int retval;

  block = but->block;
  ar = data->region;

  retval = WM_UI_HANDLER_CONTINUE;

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
                 * Causes issues with buttons defined by #uiItemFullR_with_popover. */
                block->handle->menuretval = UI_RETURN_CANCEL;
              }
              else if (ui_but_is_editable_as_text(but)) {
                ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_TEXT_EDITING);
                retval = WM_UI_HANDLER_BREAK;
              }
            }
          }
        }
        break;
      }
#endif
      case MOUSEMOVE: {
        uiBut *but_other = ui_but_find_mouse_over(ar, event);
        bool exit = false;

        /* always deactivate button for pie menus,
         * else moving to blank space will leave activated */
        if ((!ui_block_is_menu(block) || ui_block_is_pie_menu(block)) &&
            !ui_but_contains_point_px(but, ar, event->x, event->y)) {
          exit = true;
        }
        else if (but_other && ui_but_is_editable(but_other) && (but_other != but)) {
          exit = true;
        }

        if (exit) {
          data->cancel = true;
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        else if (event->x != event->prevx || event->y != event->prevy) {
          /* re-enable tooltip on mouse move */
          ui_blocks_set_tooltips(ar, true);
          button_tooltip_timer_reset(C, but);
        }

        break;
      }
      case TIMER: {
        /* Handle menu auto open timer. */
        if (event->customdata == data->autoopentimer) {
          WM_event_remove_timer(data->wm, data->window, data->autoopentimer);
          data->autoopentimer = NULL;

          if (ui_but_contains_point_px(but, ar, event->x, event->y) || but->active) {
            button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
          }
        }

        break;
      }
      /* XXX hardcoded keymap check... but anyway,
       * while view changes, tooltips should be removed */
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
             * Keep disavled for now. */
            WM_event_remove_timer(data->wm, data->window, data->hold_action_timer);
            data->hold_action_timer = NULL;
          }
          retval = WM_UI_HANDLER_CONTINUE;
          but->hold_func(C, data->region, but);
        }
        break;
      }
      case MOUSEMOVE: {
        /* deselect the button when moving the mouse away */
        /* also de-activate for buttons that only show highlights */
        if (ui_but_contains_point_px(but, ar, event->x, event->y)) {

          /* Drag on a hold button (used in the toolbar) now opens it immediately. */
          if (data->hold_action_timer) {
            if (but->flag & UI_SELECT) {
              if (len_manhattan_v2v2_int(&event->x, &event->prevx) <=
                  WM_EVENT_CURSOR_MOTION_THRESHOLD) {
                /* pass */
              }
              else {
                WM_event_remove_timer(data->wm, data->window, data->hold_action_timer);
                data->hold_action_timer = WM_event_add_timer(data->wm, data->window, TIMER, 0.0f);
              }
            }
          }

          if (!(but->flag & UI_SELECT)) {
            but->flag |= (UI_SELECT | UI_ACTIVE);
            data->cancel = false;
            ED_region_tag_redraw(data->region);
          }
        }
        else {
          if (but->flag & UI_SELECT) {
            but->flag &= ~(UI_SELECT | UI_ACTIVE);
            data->cancel = true;
            ED_region_tag_redraw(data->region);
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
    }

    retval = WM_UI_HANDLER_CONTINUE;
  }
  else if (data->state == BUTTON_STATE_MENU_OPEN) {
    /* check for exit because of mouse-over another button */
    switch (event->type) {
      case MOUSEMOVE: {
        uiBut *bt;

        if (data->menu && data->menu->region) {
          if (ui_region_contains_point_px(data->menu->region, event->x, event->y)) {
            break;
          }
        }

        bt = ui_but_find_mouse_over(ar, event);

        if (bt && bt->active != data) {
          if (but->type != UI_BTYPE_COLOR) { /* exception */
            data->cancel = true;
          }
          button_activate_state(C, but, BUTTON_STATE_EXIT);
        }
        break;
      }
      case RIGHTMOUSE: {
        if (event->val == KM_PRESS) {
          uiBut *bt = ui_but_find_mouse_over(ar, event);
          if (bt && bt->active == data) {
            button_activate_state(C, bt, BUTTON_STATE_HIGHLIGHT);
          }
        }
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
    uiButtonActivateType post_type = data->posttype;

    /* Reset the button value when empty text is typed. */
    if ((data->cancel == false) && (data->str != NULL) && (data->str[0] == '\0') &&
        (but->rnaprop && ELEM(RNA_property_type(but->rnaprop), PROP_FLOAT, PROP_INT))) {
      MEM_SAFE_FREE(data->str);
      ui_button_value_default(but, &data->value);

#ifdef USE_DRAG_MULTINUM
      if (data->multi_data.mbuts) {
        for (LinkNode *l = data->multi_data.mbuts; l; l = l->next) {
          uiButMultiState *state = l->link;
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

    button_activate_exit(C, but, data, (post_but == NULL), false);

    /* for jumping to the next button with tab while text editing */
    if (post_but) {
      button_activate_init(C, ar, post_but, post_type);
    }
    else {
      /* XXX issue is because WM_event_add_mousemove(C) is a bad hack and not reliable,
       * if that gets coded better this bypass can go away too.
       *
       * This is needed to make sure if a button was active,
       * it stays active while the mouse is over it.
       * This avoids adding mousemoves, see: [#33466] */
      if (ELEM(state_orig, BUTTON_STATE_INIT, BUTTON_STATE_HIGHLIGHT)) {
        if (ui_but_find_mouse_over(ar, event) == but) {
          button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
        }
      }
    }
  }

  return retval;
}

static int ui_handle_list_event(bContext *C, const wmEvent *event, ARegion *ar, uiBut *listbox)
{
  uiList *ui_list;
  uiListDyn *dyn_data;
  int retval = WM_UI_HANDLER_CONTINUE;
  int type = event->type, val = event->val;
  bool redraw = false;
  int mx, my;

  ui_list = listbox->custom_data;
  if (!ui_list || !ui_list->dyn_data) {
    return retval;
  }
  dyn_data = ui_list->dyn_data;

  mx = event->x;
  my = event->y;
  ui_window_to_block(ar, listbox->block, &mx, &my);

  /* convert pan to scrollwheel */
  if (type == MOUSEPAN) {
    ui_pan_to_scroll(event, &type, &val);

    /* if type still is mousepan, we call it handled, since delta-y accumulate */
    /* also see wm_event_system.c do_wheel_ui hack */
    if (type == MOUSEPAN) {
      retval = WM_UI_HANDLER_BREAK;
    }
  }

  if (val == KM_PRESS) {
    if ((ELEM(type, UPARROWKEY, DOWNARROWKEY) && !IS_EVENT_MOD(event, shift, ctrl, alt, oskey)) ||
        ((ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE) && event->ctrl &&
          !IS_EVENT_MOD(event, shift, alt, oskey)))) {
      const int value_orig = RNA_property_int_get(&listbox->rnapoin, listbox->rnaprop);
      int value, min, max, inc;

      /* activate up/down the list */
      value = value_orig;
      if ((ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) != 0) {
        inc = ELEM(type, UPARROWKEY, WHEELUPMOUSE) ? 1 : -1;
      }
      else {
        inc = ELEM(type, UPARROWKEY, WHEELUPMOUSE) ? -1 : 1;
      }

      if (dyn_data->items_filter_neworder || dyn_data->items_filter_flags) {
        /* If we have a display order different from
         * collection order, we have some work! */
        int *org_order = MEM_mallocN(dyn_data->items_shown * sizeof(int), __func__);
        const int *new_order = dyn_data->items_filter_neworder;
        int i, org_idx = -1, len = dyn_data->items_len;
        int current_idx = -1;
        int filter_exclude = ui_list->filter_flag & UILST_FLT_EXCLUDE;

        for (i = 0; i < len; i++) {
          if (!dyn_data->items_filter_flags ||
              ((dyn_data->items_filter_flags[i] & UILST_FLT_ITEM) ^ filter_exclude)) {
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
    else if (ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE) && event->shift) {
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
        ui_list->list_scroll += (type == WHEELUPMOUSE) ? -1 : 1;

        redraw = true;
        retval = WM_UI_HANDLER_BREAK;
      }
    }
  }

  if (redraw) {
    ED_region_tag_redraw(ar);
    ED_region_tag_refresh_ui(ar);
  }

  return retval;
}

static void ui_handle_button_return_submenu(bContext *C, const wmEvent *event, uiBut *but)
{
  uiHandleButtonData *data;
  uiPopupBlockHandle *menu;

  data = but->active;
  menu = data->menu;

  /* copy over return values from the closing menu */
  if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_UPDATE)) {
    if (but->type == UI_BTYPE_COLOR) {
      copy_v3_v3(data->vec, menu->retvec);
    }
    else if (but->type == UI_BTYPE_MENU) {
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
    if (event->type == MOUSEMOVE &&
        ui_but_contains_point_px(but, data->region, event->x, event->y)) {
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
 * when moving mouse towards the pulldown menu over other buttons that could
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
  BLI_assert(((uiBlock *)menu->region->uiblocks.first)->flag &
             (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER));

  if (!menu->dotowards || force) {
    menu->dotowards = true;
    menu->towards_xy[0] = xy[0];
    menu->towards_xy[1] = xy[1];

    if (force) {
      menu->towardstime = DBL_MAX; /* unlimited time */
    }
    else {
      menu->towardstime = PIL_check_seconds_timer();
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
  float p1[2], p2[2], p3[2], p4[2];
  float oldp[2] = {menu->towards_xy[0], menu->towards_xy[1]};
  const float newp[2] = {xy[0], xy[1]};
  bool closer;
  const float margin = MENU_TOWARDS_MARGIN;
  rctf rect_px;

  BLI_assert(block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER));

  /* annoying fix for [#36269], this is a bit odd but in fact works quite well
   * don't mouse-out of a menu if another menu has been created after it.
   * if this causes problems we could remove it and check on a different fix - campbell */
  if (menu->region->next) {
    /* am I the last menu (test) */
    ARegion *ar = menu->region->next;
    do {
      uiBlock *block_iter = ar->uiblocks.first;
      if (block_iter && ui_block_is_menu(block_iter)) {
        return true;
      }
    } while ((ar = ar->next));
  }
  /* annoying fix end! */

  if (!menu->dotowards) {
    return false;
  }

  if (len_squared_v2v2(oldp, newp) < (4.0f * 4.0f)) {
    return menu->dotowards;
  }

  /* verify that we are moving towards one of the edges of the
   * menu block, in other words, in the triangle formed by the
   * initial mouse location and two edge points. */
  ui_block_to_window_rctf(menu->region, block, &rect_px, &block->rect);

  p1[0] = rect_px.xmin - margin;
  p1[1] = rect_px.ymin - margin;

  p2[0] = rect_px.xmax + margin;
  p2[1] = rect_px.ymin - margin;

  p3[0] = rect_px.xmax + margin;
  p3[1] = rect_px.ymax + margin;

  p4[0] = rect_px.xmin - margin;
  p4[1] = rect_px.ymax + margin;

  /* allow for some wiggle room, if the user moves a few pixels away,
   * don't immediately quit (only for top level menus) */
  if (use_wiggle_room) {
    const float cent[2] = {BLI_rctf_cent_x(&rect_px), BLI_rctf_cent_y(&rect_px)};
    float delta[2];

    sub_v2_v2v2(delta, oldp, cent);
    normalize_v2_length(delta, MENU_TOWARDS_WIGGLE_ROOM);
    add_v2_v2(oldp, delta);
  }

  closer = (isect_point_tri_v2(newp, oldp, p1, p2) || isect_point_tri_v2(newp, oldp, p2, p3) ||
            isect_point_tri_v2(newp, oldp, p3, p4) || isect_point_tri_v2(newp, oldp, p4, p1));

  if (!closer) {
    menu->dotowards = false;
  }

  /* 1 second timer */
  if (PIL_check_seconds_timer() - menu->towardstime > BUTTON_MOUSE_TOWARDS_THRESH) {
    menu->dotowards = false;
  }

  return menu->dotowards;
}

#ifdef USE_KEYNAV_LIMIT
static void ui_mouse_motion_keynav_init(struct uiKeyNavLock *keynav, const wmEvent *event)
{
  keynav->is_keynav = true;
  copy_v2_v2_int(keynav->event_xy, &event->x);
}
/**
 * Return true if keyinput isn't blocking mouse-motion,
 * or if the mouse-motion is enough to disable keyinput.
 */
static bool ui_mouse_motion_keynav_test(struct uiKeyNavLock *keynav, const wmEvent *event)
{
  if (keynav->is_keynav &&
      (len_manhattan_v2v2_int(keynav->event_xy, &event->x) > BUTTON_KEYNAV_PX_LIMIT)) {
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

static void ui_menu_scroll_apply_offset_y(ARegion *ar, uiBlock *block, float dy)
{
  BLI_assert(dy != 0.0f);
  if (dy < 0.0f) {
    /* stop at top item, extra 0.5 unit Y makes it snap nicer */
    float ymax = -FLT_MAX;
    for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
      ymax = max_ff(ymax, bt->rect.ymax);
    }
    if (ymax + dy - UI_UNIT_Y * 0.5f < block->rect.ymax - UI_MENU_SCROLL_PAD) {
      dy = block->rect.ymax - ymax - UI_MENU_SCROLL_PAD;
    }
  }
  else {
    /* stop at bottom item, extra 0.5 unit Y makes it snap nicer */
    float ymin = FLT_MAX;
    for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
      ymin = min_ff(ymin, bt->rect.ymin);
    }
    if (ymin + dy + UI_UNIT_Y * 0.5f > block->rect.ymin + UI_MENU_SCROLL_PAD) {
      dy = block->rect.ymin - ymin + UI_MENU_SCROLL_PAD;
    }
  }

  /* remember scroll offset for refreshes */
  block->handle->scrolloffset += dy;

  /* apply scroll offset */
  for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
    bt->rect.ymin += dy;
    bt->rect.ymax += dy;
  }

  /* set flags again */
  ui_popup_block_scrolltest(block);

  ED_region_tag_redraw(ar);
}

/** Scroll to activated button. */
static bool ui_menu_scroll_to_but(ARegion *ar, uiBlock *block, uiBut *but_target)
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
    ui_menu_scroll_apply_offset_y(ar, block, dy);
    return true;
  }
  return false;
}

/** Scroll to y location (in block space, see #ui_window_to_block). */
static bool ui_menu_scroll_to_y(ARegion *ar, uiBlock *block, int y)
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
    ui_menu_scroll_apply_offset_y(ar, block, dy);
    return true;
  }
  return false;
}

static bool ui_menu_scroll_step(ARegion *ar, uiBlock *block, const int scroll_dir)
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

  return ui_menu_scroll_to_y(ar, block, my);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Event Handling
 * \{ */

static void ui_region_auto_open_clear(ARegion *ar)
{
  for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
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
 * Without this keyboard navigation from menu's wont work.
 */
static bool ui_menu_pass_event_to_parent_if_nonactive(uiPopupBlockHandle *menu,
                                                      const uiBut *but,
                                                      const int level,
                                                      const int retval)
{
  if ((level != 0) && (but == NULL)) {
    menu->menuretval = UI_RETURN_OUT | UI_RETURN_OUT_PARENT;
    (void)retval; /* so release builds with strict flags are happy as well */
    BLI_assert(retval == WM_UI_HANDLER_CONTINUE);
    return true;
  }
  else {
    return false;
  }
}

static int ui_handle_menu_button(bContext *C, const wmEvent *event, uiPopupBlockHandle *menu)
{
  ARegion *ar = menu->region;
  uiBut *but = ui_region_find_active_but(ar);
  int retval;

  if (but) {
    /* Its possible there is an active menu item NOT under the mouse,
     * in this case ignore mouse clicks outside the button (but Enter etc is accepted) */
    if (event->val == KM_RELEASE) {
      /* pass, needed so we can exit active menu-items when click-dragging out of them */
    }
    else if (!ui_block_is_menu(but->block) || ui_block_is_pie_menu(but->block)) {
      /* pass, skip for dialogs */
    }
    else if (!ui_region_contains_point_px(but->active->region, event->x, event->y)) {
      /* pass, needed to click-exit outside of non-flaoting menus */
      ui_region_auto_open_clear(but->active->region);
    }
    else if ((!ELEM(event->type, MOUSEMOVE, WHEELUPMOUSE, WHEELDOWNMOUSE, MOUSEPAN)) &&
             ISMOUSE(event->type)) {
      if (!ui_but_contains_point_px(but, but->active->region, event->x, event->y)) {
        but = NULL;
      }
    }
  }

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
    retval = ui_handle_button_over(C, event, ar);
  }

  return retval;
}

float ui_block_calc_pie_segment(uiBlock *block, const float event_xy[2])
{
  float seg1[2];
  float seg2[2];
  float len;

  if (block->pie_data.flags & UI_PIE_INITIAL_DIRECTION) {
    copy_v2_v2(seg1, block->pie_data.pie_center_init);
  }
  else {
    copy_v2_v2(seg1, block->pie_data.pie_center_spawned);
  }

  sub_v2_v2v2(seg2, event_xy, seg1);

  len = normalize_v2_v2(block->pie_data.pie_dir, seg2);

  if (len < U.pie_menu_threshold * U.dpi_fac) {
    block->pie_data.flags |= UI_PIE_INVALID_DIR;
  }
  else {
    block->pie_data.flags &= ~UI_PIE_INVALID_DIR;
  }

  return len;
}

static int ui_handle_menu_event(bContext *C,
                                const wmEvent *event,
                                uiPopupBlockHandle *menu,
                                int level,
                                const bool is_parent_inside,
                                const bool is_parent_menu,
                                const bool is_floating)
{
  ARegion *ar;
  uiBlock *block;
  uiBut *but;
  int mx, my, retval;
  bool inside;
  bool inside_title; /* check for title dragging */

  ar = menu->region;
  block = ar->uiblocks.first;

  retval = WM_UI_HANDLER_CONTINUE;

  mx = event->x;
  my = event->y;
  ui_window_to_block(ar, block, &mx, &my);

  /* check if mouse is inside block */
  inside = BLI_rctf_isect_pt(&block->rect, mx, my);
  inside_title = inside && ((my + (UI_UNIT_Y * 1.5f)) > block->rect.ymax);

  /* if there's an active modal button, don't check events or outside, except for search menu */
  but = ui_region_find_active_but(ar);

#ifdef USE_DRAG_POPUP
  if (menu->is_grab) {
    if (event->type == LEFTMOUSE) {
      menu->is_grab = false;
      retval = WM_UI_HANDLER_BREAK;
    }
    else {
      if (event->type == MOUSEMOVE) {
        int mdiff[2];

        sub_v2_v2v2_int(mdiff, &event->x, menu->grab_xy_prev);
        copy_v2_v2_int(menu->grab_xy_prev, &event->x);

        add_v2_v2v2_int(menu->popup_create_vars.event_xy, menu->popup_create_vars.event_xy, mdiff);

        ui_popup_translate(ar, mdiff);
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
      ui_mouse_motion_towards_reinit(menu, &event->x);
    }
  }
  else if (event->type == TIMER) {
    if (event->customdata == menu->scrolltimer) {
      ui_menu_scroll_to_y(ar, block, my);
    }
  }
  else {
    /* for ui_mouse_motion_towards_block */
    if (event->type == MOUSEMOVE) {
      if (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER)) {
        ui_mouse_motion_towards_init(menu, &event->x);
      }

      /* add menu scroll timer, if needed */
      if (ui_menu_scroll_test(block, my)) {
        if (menu->scrolltimer == NULL) {
          menu->scrolltimer = WM_event_add_timer(
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
         * This is done so we can right click on menu items even when they have sub-menus open. */
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
                menu->menuretval = (level > 0) ? UI_RETURN_OUT_PARENT : UI_RETURN_OUT;
              }
            }
            retval = WM_UI_HANDLER_BREAK;
          }
          break;

        /* Closing sub-levels of pull-downs. */
        case LEFTARROWKEY:
          if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP)) {
            if (block->saferct.first) {
              menu->menuretval = UI_RETURN_OUT;
            }
          }

          retval = WM_UI_HANDLER_BREAK;
          break;

        /* Opening sub-levels of pull-downs. */
        case RIGHTARROWKEY:
          if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP)) {

            if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval)) {
              break;
            }

            but = ui_region_find_active_but(ar);

            if (!but) {
              /* no item active, we make first active */
              if (block->direction & UI_DIR_UP) {
                but = ui_but_last(block);
              }
              else {
                but = ui_but_first(block);
              }
            }

            if (but && ELEM(but->type, UI_BTYPE_BLOCK, UI_BTYPE_PULLDOWN)) {
              ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_OPEN);
            }
          }

          retval = WM_UI_HANDLER_BREAK;
          break;

        case WHEELUPMOUSE:
        case WHEELDOWNMOUSE: {
          if (IS_EVENT_MOD(event, shift, ctrl, alt, oskey)) {
            /* pass */
          }
          else if (!ui_block_is_menu(block)) {
            const int scroll_dir = (event->type == WHEELUPMOUSE) ? 1 : -1;
            if (ui_menu_scroll_step(ar, block, scroll_dir)) {
              if (but) {
                but->active->cancel = true;
                button_activate_exit(C, but, but->active, false, false);
              }
              WM_event_add_mousemove(C);
            }
            break;
          }
          ATTR_FALLTHROUGH;
        }
        case UPARROWKEY:
        case DOWNARROWKEY:
        case MOUSEPAN:
          /* arrowkeys: only handle for block_loop blocks */
          if (IS_EVENT_MOD(event, shift, ctrl, alt, oskey)) {
            /* pass */
          }
          else if (inside || (block->flag & UI_BLOCK_LOOP)) {
            int type = event->type;
            int val = event->val;

            /* convert pan to scrollwheel */
            if (type == MOUSEPAN) {
              ui_pan_to_scroll(event, &type, &val);
            }

            if (val == KM_PRESS) {
              const bool is_next = (ELEM(type, DOWNARROWKEY, WHEELDOWNMOUSE) ==
                                    ((block->flag & UI_BLOCK_IS_FLIP) != 0));

              if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval)) {
                break;
              }

#ifdef USE_KEYNAV_LIMIT
              ui_mouse_motion_keynav_init(&menu->keynav_state, event);
#endif

              but = ui_region_find_active_but(ar);
              if (but) {
                /* next button */
                but = is_next ? ui_but_next(but) : ui_but_prev(but);
              }

              if (!but) {
                /* wrap button */
                uiBut *but_wrap;
                but_wrap = is_next ? ui_but_first(block) : ui_but_last(block);
                if (but_wrap) {
                  but = but_wrap;
                }
              }

              if (but) {
                ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE);
                ui_menu_scroll_to_but(ar, block, but);
              }
            }

            retval = WM_UI_HANDLER_BREAK;
          }

          break;

        case ONEKEY:
        case PAD1:
          act = 1;
          ATTR_FALLTHROUGH;
        case TWOKEY:
        case PAD2:
          if (act == 0) {
            act = 2;
          }
          ATTR_FALLTHROUGH;
        case THREEKEY:
        case PAD3:
          if (act == 0) {
            act = 3;
          }
          ATTR_FALLTHROUGH;
        case FOURKEY:
        case PAD4:
          if (act == 0) {
            act = 4;
          }
          ATTR_FALLTHROUGH;
        case FIVEKEY:
        case PAD5:
          if (act == 0) {
            act = 5;
          }
          ATTR_FALLTHROUGH;
        case SIXKEY:
        case PAD6:
          if (act == 0) {
            act = 6;
          }
          ATTR_FALLTHROUGH;
        case SEVENKEY:
        case PAD7:
          if (act == 0) {
            act = 7;
          }
          ATTR_FALLTHROUGH;
        case EIGHTKEY:
        case PAD8:
          if (act == 0) {
            act = 8;
          }
          ATTR_FALLTHROUGH;
        case NINEKEY:
        case PAD9:
          if (act == 0) {
            act = 9;
          }
          ATTR_FALLTHROUGH;
        case ZEROKEY:
        case PAD0:
          if (act == 0) {
            act = 10;
          }

          if ((block->flag & UI_BLOCK_NUMSELECT) && event->val == KM_PRESS) {
            int count;

            if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval)) {
              break;
            }

            if (event->alt) {
              act += 10;
            }

            count = 0;
            for (but = block->buttons.first; but; but = but->next) {
              bool doit = false;

              if (!ELEM(but->type, UI_BTYPE_LABEL, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE)) {
                count++;
              }

              /* exception for rna layer buts */
              if (but->rnapoin.data && but->rnaprop &&
                  ELEM(RNA_property_subtype(but->rnaprop), PROP_LAYER, PROP_LAYER_MEMBER)) {
                if (but->rnaindex == act - 1) {
                  doit = true;
                }
              }
              else if (ELEM(but->type,
                            UI_BTYPE_BUT,
                            UI_BTYPE_BUT_MENU,
                            UI_BTYPE_MENU,
                            UI_BTYPE_BLOCK,
                            UI_BTYPE_PULLDOWN) &&
                       count == act) {
                doit = true;
              }

              if (!(but->flag & UI_BUT_DISABLED) && doit) {
                /* activate buttons but open menu's */
                uiButtonActivateType activate;
                if (but->type == UI_BTYPE_PULLDOWN) {
                  activate = BUTTON_ACTIVATE_OPEN;
                }
                else {
                  activate = BUTTON_ACTIVATE_APPLY;
                }

                ui_handle_button_activate(C, ar, but, activate);
                break;
              }
            }

            retval = WM_UI_HANDLER_BREAK;
          }
          break;

        /* Handle keystrokes on menu items */
        case AKEY:
        case BKEY:
        case CKEY:
        case DKEY:
        case EKEY:
        case FKEY:
        case GKEY:
        case HKEY:
        case IKEY:
        case JKEY:
        case KKEY:
        case LKEY:
        case MKEY:
        case NKEY:
        case OKEY:
        case PKEY:
        case QKEY:
        case RKEY:
        case SKEY:
        case TKEY:
        case UKEY:
        case VKEY:
        case WKEY:
        case XKEY:
        case YKEY:
        case ZKEY: {
          if ((event->val == KM_PRESS || event->val == KM_DBL_CLICK) &&
              !IS_EVENT_MOD(event, shift, ctrl, oskey)) {
            if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval)) {
              break;
            }

            for (but = block->buttons.first; but; but = but->next) {
              if (!(but->flag & UI_BUT_DISABLED) && but->menu_key == event->type) {
                if (but->type == UI_BTYPE_BUT) {
                  UI_but_execute(C, but);
                }
                else {
                  ui_handle_button_activate_by_type(C, ar, but);
                }
                break;
              }
            }

            retval = WM_UI_HANDLER_BREAK;
          }
          break;
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
       * don't overwrite them, see: T61015.
       */
      if ((inside == 0) && (menu->menuretval == 0)) {
        uiSafetyRct *saferct = block->saferct.first;

        if (ELEM(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE)) {
          if (ELEM(event->val, KM_PRESS, KM_DBL_CLICK)) {
            if ((is_parent_menu == false) && (U.uiflag & USER_MENUOPENAUTO) == 0) {
              /* for root menus, allow clicking to close */
              if (block->flag & (UI_BLOCK_OUT_1)) {
                menu->menuretval = UI_RETURN_OK;
              }
              else {
                menu->menuretval = UI_RETURN_OUT;
              }
            }
            else if (saferct && !BLI_rctf_isect_pt(&saferct->parent, event->x, event->y)) {
              if (block->flag & (UI_BLOCK_OUT_1)) {
                menu->menuretval = UI_RETURN_OK;
              }
              else {
                menu->menuretval = UI_RETURN_OUT;
              }
            }
          }
          else if (ELEM(event->val, KM_RELEASE, KM_CLICK)) {
            /* For buttons that use a hold function,
             * exit when mouse-up outside the menu. */
            if (block->flag & UI_BLOCK_POPUP_HOLD) {
              /* Note, we could check the cursor is over the parent button. */
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
               ui_mouse_motion_keynav_test(&menu->keynav_state, event)) {
        /* Don't handle the mouse-move if we're using key-navigation. */
        retval = WM_UI_HANDLER_BREAK;
      }
#endif
      else if (event->type == ESCKEY && event->val == KM_PRESS) {
        /* Escape cancels this and all preceding menus. */
        menu->menuretval = UI_RETURN_CANCEL;
      }
      else if (ELEM(event->type, RETKEY, PADENTER) && event->val == KM_PRESS) {
        uiBut *but_default = ui_region_find_first_but_test_flag(
            ar, UI_BUT_ACTIVE_DEFAULT, UI_HIDDEN);
        if ((but_default != NULL) && (but_default->active == NULL)) {
          if (but_default->type == UI_BTYPE_BUT) {
            UI_but_execute(C, but_default);
          }
          else {
            ui_handle_button_activate_by_type(C, ar, but_default);
          }
        }
        else {
          uiBut *but_active = ui_region_find_active_but(ar);

          /* enter will always close this block, we let the event
           * get handled by the button if it is activated, otherwise we cancel */
          if (but_active == NULL) {
            menu->menuretval = UI_RETURN_CANCEL | UI_RETURN_POPUP_OK;
          }
        }
      }
#ifdef USE_DRAG_POPUP
      else if ((event->type == LEFTMOUSE) && (event->val == KM_PRESS) &&
               (inside && is_floating && inside_title)) {
        if (!but || !ui_but_contains_point_px(but, ar, event->x, event->y)) {
          if (but) {
            UI_but_tooltip_timer_remove(C, but);
          }

          menu->is_grab = true;
          copy_v2_v2_int(menu->grab_xy_prev, &event->x);
          retval = WM_UI_HANDLER_BREAK;
        }
      }
#endif
      else {

        /* check mouse moving outside of the menu */
        if (inside == 0 && (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER))) {
          uiSafetyRct *saferct;

          ui_mouse_motion_towards_check(block, menu, &event->x, is_parent_inside == false);

          /* check for all parent rects, enables arrowkeys to be used */
          for (saferct = block->saferct.first; saferct; saferct = saferct->next) {
            /* for mouse move we only check our own rect, for other
             * events we check all preceding block rects too to make
             * arrow keys navigation work */
            if (event->type != MOUSEMOVE || saferct == block->saferct.first) {
              if (BLI_rctf_isect_pt(&saferct->parent, (float)event->x, (float)event->y)) {
                break;
              }
              if (BLI_rctf_isect_pt(&saferct->safety, (float)event->x, (float)event->y)) {
                break;
              }
            }
          }

          /* strict check, and include the parent rect */
          if (!menu->dotowards && !saferct) {
            if (block->flag & (UI_BLOCK_OUT_1)) {
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
      (/*inside &&*/ (!menu->menuretval || (menu->menuretval & UI_RETURN_UPDATE)) &&
       retval == WM_UI_HANDLER_CONTINUE)) {
    retval = ui_handle_menu_button(C, event, menu);
  }

#ifdef USE_UI_POPOVER_ONCE
  if (block->flag & UI_BLOCK_POPOVER_ONCE) {
    if ((event->type == LEFTMOUSE) && (event->val == KM_RELEASE)) {
      UI_popover_once_clear(menu->popup_create_vars.arg);
      block->flag &= ~UI_BLOCK_POPOVER_ONCE;
    }
  }
#endif

  /* Don't handle double click events, rehandle as regular press/release. */
  if (retval == WM_UI_HANDLER_CONTINUE && event->val == KM_DBL_CLICK) {
    return retval;
  }

  /* if we set a menu return value, ensure we continue passing this on to
   * lower menus and buttons, so always set continue then, and if we are
   * inside the region otherwise, ensure we swallow the event */
  if (menu->menuretval) {
    return WM_UI_HANDLER_CONTINUE;
  }
  else if (inside) {
    return WM_UI_HANDLER_BREAK;
  }
  else {
    return retval;
  }
}

static int ui_handle_menu_return_submenu(bContext *C,
                                         const wmEvent *event,
                                         uiPopupBlockHandle *menu)
{
  ARegion *ar;
  uiBut *but;
  uiBlock *block;
  uiHandleButtonData *data;
  uiPopupBlockHandle *submenu;

  ar = menu->region;
  block = ar->uiblocks.first;

  but = ui_region_find_active_but(ar);

  BLI_assert(but);

  data = but->active;
  submenu = data->menu;

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
    ui_mouse_motion_towards_reinit(menu, &event->x);
  }

  if (menu->menuretval) {
    return WM_UI_HANDLER_CONTINUE;
  }
  else {
    return WM_UI_HANDLER_BREAK;
  }
}

static bool ui_but_pie_menu_supported_apply(uiBut *but)
{
  return (!ELEM(but->type, UI_BTYPE_NUM_SLIDER, UI_BTYPE_NUM));
}

static int ui_but_pie_menu_apply(bContext *C,
                                 uiPopupBlockHandle *menu,
                                 uiBut *but,
                                 bool force_close)
{
  int retval = WM_UI_HANDLER_BREAK;

  if (but && ui_but_pie_menu_supported_apply(but)) {
    if (but->type == UI_BTYPE_MENU) {
      /* forcing the pie menu to close will not handle menus */
      if (!force_close) {
        uiBut *active_but = ui_region_find_active_but(menu->region);

        if (active_but) {
          button_activate_exit(C, active_but, active_but->active, false, false);
        }

        button_activate_init(C, menu->region, but, BUTTON_ACTIVATE_OPEN);
        return retval;
      }
      else {
        menu->menuretval = UI_RETURN_CANCEL;
      }
    }
    else {
      ui_apply_but(C, but->block, but, but->active, false);
      button_activate_exit((bContext *)C, but, but->active, false, true);

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
  uiBut *but;

  if ((block->flag & UI_BLOCK_NUMSELECT) && event->val == KM_PRESS) {
    for (but = block->buttons.first; but; but = but->next) {
      if (but->pie_dir == dir && !ELEM(but->type, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE)) {
        return but;
      }
    }
  }

  return NULL;
}

static int ui_but_pie_button_activate(bContext *C, uiBut *but, uiPopupBlockHandle *menu)
{
  uiBut *active_but;

  if (but == NULL) {
    return WM_UI_HANDLER_BREAK;
  }

  active_but = ui_region_find_active_but(menu->region);

  if (active_but) {
    button_activate_exit(C, active_but, active_but->active, false, false);
  }

  button_activate_init(C, menu->region, but, BUTTON_ACTIVATE_OVER);
  return ui_but_pie_menu_apply(C, menu, but, false);
}

static int ui_pie_handler(bContext *C, const wmEvent *event, uiPopupBlockHandle *menu)
{
  ARegion *ar;
  uiBlock *block;
  uiBut *but;
  float event_xy[2];
  double duration;
  bool is_click_style;
  float dist;

  /* we block all events, this is modal interaction,
   * except for drop events which is described below */
  int retval = WM_UI_HANDLER_BREAK;

  if (event->type == EVT_DROP) {
    /* may want to leave this here for later if we support pie ovens */

    retval = WM_UI_HANDLER_CONTINUE;
  }

  ar = menu->region;
  block = ar->uiblocks.first;

  is_click_style = (block->pie_data.flags & UI_PIE_CLICK_STYLE);

  /* if there's an active modal button, don't check events or outside, except for search menu */
  but = ui_region_find_active_but(ar);

  if (menu->scrolltimer == NULL) {
    menu->scrolltimer = WM_event_add_timer(
        CTX_wm_manager(C), CTX_wm_window(C), TIMER, PIE_MENU_INTERVAL);
    menu->scrolltimer->duration = 0.0;
  }

  duration = menu->scrolltimer->duration;

  event_xy[0] = event->x;
  event_xy[1] = event->y;

  ui_window_to_block_fl(ar, block, &event_xy[0], &event_xy[1]);

  /* Distance from initial point. */
  dist = ui_block_calc_pie_segment(block, event_xy);

  if (but && button_modal_state(but->active->state)) {
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
          double final_time = 0.01 * U.pie_animation_timeout;
          float fac = duration / final_time;
          float pie_radius = U.pie_menu_radius * UI_DPI_FAC;

          if (fac > 1.0f) {
            fac = 1.0f;
            block->pie_data.flags |= UI_PIE_ANIMATION_FINISHED;
          }

          for (but = block->buttons.first; but; but = but->next) {
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

          ED_region_tag_redraw(ar);
        }
      }

      /* check pie velociy here if gesture has ended */
      if (block->pie_data.flags & UI_PIE_GESTURE_END_WAIT) {
        float len_sq = 10;

        /* use a time threshold to ensure we leave time to the mouse to move */
        if (duration - block->pie_data.duration_gesture > 0.02) {
          len_sq = len_squared_v2v2(event_xy, block->pie_data.last_pos);
          copy_v2_v2(block->pie_data.last_pos, event_xy);
          block->pie_data.duration_gesture = duration;
        }

        if (len_sq < 1.0f) {
          but = ui_region_find_active_but(menu->region);

          if (but) {
            return ui_but_pie_menu_apply(C, menu, but, true);
          }
        }
      }
    }

    if (event->type == block->pie_data.event && !is_click_style) {
      if (event->val != KM_RELEASE) {
        ui_handle_menu_button(C, event, menu);

        if (len_squared_v2v2(event_xy, block->pie_data.pie_center_init) > PIE_CLICK_THRESHOLD_SQ) {
          block->pie_data.flags |= UI_PIE_DRAG_STYLE;
        }
        /* why redraw here? It's simple, we are getting many double click events here.
         * Those operate like mouse move events almost */
        ED_region_tag_redraw(ar);
      }
      else {
        if ((duration < 0.01 * U.pie_tap_timeout) &&
            !(block->pie_data.flags & UI_PIE_DRAG_STYLE)) {
          block->pie_data.flags |= UI_PIE_CLICK_STYLE;
        }
        else {
          but = ui_region_find_active_but(menu->region);

          if (but && (U.pie_menu_confirm > 0) &&
              (dist >= U.dpi_fac * (U.pie_menu_threshold + U.pie_menu_confirm))) {
            if (but) {
              return ui_but_pie_menu_apply(C, menu, but, true);
            }
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
            float len_sq = len_squared_v2v2(event_xy, block->pie_data.pie_center_init);

            /* here we use the initial position explicitly */
            if (len_sq > PIE_CLICK_THRESHOLD_SQ) {
              block->pie_data.flags |= UI_PIE_DRAG_STYLE;
            }

            /* here instead, we use the offset location to account for the initial
             * direction timeout */
            if ((U.pie_menu_confirm > 0) &&
                (dist >= U.dpi_fac * (U.pie_menu_threshold + U.pie_menu_confirm))) {
              block->pie_data.flags |= UI_PIE_GESTURE_END_WAIT;
              copy_v2_v2(block->pie_data.last_pos, event_xy);
              block->pie_data.duration_gesture = duration;
            }
          }

          ui_handle_menu_button(C, event, menu);

          /* mouse move should always refresh the area for pie menus */
          ED_region_tag_redraw(ar);
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

        case ESCKEY:
        case RIGHTMOUSE:
          menu->menuretval = UI_RETURN_CANCEL;
          break;

        case AKEY:
        case BKEY:
        case CKEY:
        case DKEY:
        case EKEY:
        case FKEY:
        case GKEY:
        case HKEY:
        case IKEY:
        case JKEY:
        case KKEY:
        case LKEY:
        case MKEY:
        case NKEY:
        case OKEY:
        case PKEY:
        case QKEY:
        case RKEY:
        case SKEY:
        case TKEY:
        case UKEY:
        case VKEY:
        case WKEY:
        case XKEY:
        case YKEY:
        case ZKEY: {
          if ((event->val == KM_PRESS || event->val == KM_DBL_CLICK) &&
              !IS_EVENT_MOD(event, shift, ctrl, oskey)) {
            for (but = block->buttons.first; but; but = but->next) {
              if (but->menu_key == event->type) {
                ui_but_pie_button_activate(C, but, menu);
              }
            }
          }
          break;
        }

#define CASE_NUM_TO_DIR(n, d) \
  case (ZEROKEY + n): \
  case (PAD0 + n): { \
    if (num_dir == UI_RADIAL_NONE) \
      num_dir = d; \
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
            but = ui_block_pie_dir_activate(block, event, num_dir);
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
  uiBut *but;
  uiHandleButtonData *data;
  uiPopupBlockHandle *submenu;
  int retval = WM_UI_HANDLER_CONTINUE;
  bool do_towards_reinit = false;

  /* check if we have a submenu, and handle events for it first */
  but = ui_region_find_active_but(menu->region);
  data = (but) ? but->active : NULL;
  submenu = (data) ? data->menu : NULL;

  if (submenu) {
    uiBlock *block = menu->region->uiblocks.first;
    const bool is_menu = ui_block_is_menu(block);
    bool inside = false;
    /* root pie menus accept the key that spawned
     * them as double click to improve responsiveness */
    bool do_recursion = (!(block->flag & UI_BLOCK_RADIAL) || event->type != block->pie_data.event);

    if (do_recursion) {
      if (is_parent_inside == false) {
        int mx, my;

        mx = event->x;
        my = event->y;
        ui_window_to_block(menu->region, block, &mx, &my);
        inside = BLI_rctf_isect_pt(&block->rect, mx, my);
      }

      retval = ui_handle_menus_recursive(
          C, event, submenu, level + 1, is_parent_inside || inside, is_menu, false);
    }
  }

  /* now handle events for our own menu */
  if (retval == WM_UI_HANDLER_CONTINUE || event->type == TIMER) {
    const bool do_but_search = (but && (but->type == UI_BTYPE_SEARCH_MENU));
    if (submenu && submenu->menuretval) {
      const bool do_ret_out_parent = (submenu->menuretval & UI_RETURN_OUT_PARENT) != 0;
      retval = ui_handle_menu_return_submenu(C, event, menu);
      submenu = NULL; /* hint not to use this, it may be freed by call above */
      (void)submenu;
      /* we may want to quit the submenu and handle the even in this menu,
       * if its important to use it, check 'data->menu' first */
      if (((retval == WM_UI_HANDLER_BREAK) && do_ret_out_parent) == 0) {
        /* skip applying the event */
        return retval;
      }
    }

    if (do_but_search) {
      uiBlock *block = menu->region->uiblocks.first;

      retval = ui_handle_menu_button(C, event, menu);

      if (block->flag & (UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPOVER)) {
        /* when there is a active search button and we close it,
         * we need to reinit the mouse coords [#35346] */
        if (ui_region_find_active_but(menu->region) != but) {
          do_towards_reinit = true;
        }
      }
    }
    else {
      uiBlock *block = menu->region->uiblocks.first;
      uiBut *listbox = ui_list_find_mouse_over(menu->region, event);

      if (block->flag & UI_BLOCK_RADIAL) {
        retval = ui_pie_handler(C, event, menu);
      }
      else if (event->type == LEFTMOUSE || event->val != KM_DBL_CLICK) {
        bool handled = false;

        if (listbox) {
          int retval_test = ui_handle_list_event(C, event, menu->region, listbox);
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

  if (do_towards_reinit) {
    ui_mouse_motion_towards_reinit(menu, &event->x);
  }

  return retval;
}

/**
 * Allow setting menu return value from externals.
 * E.g. WM might need to do this for exiting files correctly.
 */
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

static int ui_region_handler(bContext *C, const wmEvent *event, void *UNUSED(userdata))
{
  ARegion *ar;
  uiBut *but, *listbox;
  int retval;

  /* here we handle buttons at the region level, non-modal */
  ar = CTX_wm_region(C);
  retval = WM_UI_HANDLER_CONTINUE;

  if (ar == NULL || BLI_listbase_is_empty(&ar->uiblocks)) {
    return retval;
  }

  /* either handle events for already activated button or try to activate */
  but = ui_region_find_active_but(ar);
  listbox = ui_list_find_mouse_over(ar, event);

  retval = ui_handler_panel_region(C, event, ar, listbox ? listbox : but);

  if (retval == WM_UI_HANDLER_CONTINUE && listbox) {
    retval = ui_handle_list_event(C, event, ar, listbox);

    /* interactions with the listbox should disable tips */
    if (retval == WM_UI_HANDLER_BREAK) {
      if (but) {
        UI_but_tooltip_timer_remove(C, but);
      }
    }
  }

  if (retval == WM_UI_HANDLER_CONTINUE) {
    if (but) {
      retval = ui_handle_button_event(C, event, but);
    }
    else {
      retval = ui_handle_button_over(C, event, ar);
    }
  }

  /* re-enable tooltips */
  if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy)) {
    ui_blocks_set_tooltips(ar, true);
  }

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);

  return retval;
}

static void ui_region_handler_remove(bContext *C, void *UNUSED(userdata))
{
  bScreen *sc;
  ARegion *ar;

  ar = CTX_wm_region(C);
  if (ar == NULL) {
    return;
  }

  UI_blocklist_free(C, &ar->uiblocks);

  sc = CTX_wm_screen(C);
  if (sc == NULL) {
    return;
  }

  /* delayed apply callbacks, but not for screen level regions, those
   * we rather do at the very end after closing them all, which will
   * be done in ui_region_handler/window */
  if (BLI_findindex(&sc->regionbase, ar) == -1) {
    ui_apply_but_funcs_after(C);
  }
}

/* handle buttons at the window level, modal, for example while
 * number sliding, text editing, or when a menu block is open */
static int ui_handler_region_menu(bContext *C, const wmEvent *event, void *UNUSED(userdata))
{
  ARegion *ar;
  uiBut *but;
  int retval = WM_UI_HANDLER_CONTINUE;

  ar = CTX_wm_menu(C);
  if (!ar) {
    ar = CTX_wm_region(C);
  }

  but = ui_region_find_active_but(ar);

  if (but) {
    bScreen *screen = CTX_wm_screen(C);
    ARegion *ar_temp;
    uiBut *but_other;
    uiHandleButtonData *data;
    bool is_inside_menu = false;

    /* look for a popup menu containing the mouse */
    for (ar_temp = screen->regionbase.first; ar_temp; ar_temp = ar_temp->next) {
      rcti winrct;

      ui_region_winrct_get_no_margin(ar_temp, &winrct);

      if (BLI_rcti_isect_pt_v(&winrct, &event->x)) {
        BLI_assert(ar_temp->type->regionid == RGN_TYPE_TEMPORARY);

        is_inside_menu = true;
        break;
      }
    }

    /* handle activated button events */
    data = but->active;

    if ((data->state == BUTTON_STATE_MENU_OPEN) &&
        /* make sure mouse isn't inside another menu (see T43247) */
        (is_inside_menu == false) && (ELEM(but->type, UI_BTYPE_PULLDOWN, UI_BTYPE_POPOVER)) &&
        (but_other = ui_but_find_mouse_over(ar, event)) && (but != but_other) &&
        (ELEM(but_other->type, UI_BTYPE_PULLDOWN, UI_BTYPE_POPOVER))) {
      /* if mouse moves to a different root-level menu button,
       * open it to replace the current menu */
      if ((but_other->flag & UI_BUT_DISABLED) == 0) {
        ui_handle_button_activate(C, ar, but_other, BUTTON_ACTIVATE_OVER);
        button_activate_state(C, but_other, BUTTON_STATE_MENU_OPEN);
        retval = WM_UI_HANDLER_BREAK;
      }
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

  /* re-enable tooltips */
  if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy)) {
    ui_blocks_set_tooltips(ar, true);
  }

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);

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
  uiPopupBlockHandle *menu = userdata;
  struct ARegion *menu_region;
  /* we block all events, this is modal interaction,
   * except for drop events which is described below */
  int retval = WM_UI_HANDLER_BREAK;
  bool reset_pie = false;

  menu_region = CTX_wm_menu(C);
  CTX_wm_menu_set(C, menu->region);

  if (event->type == EVT_DROP || event->val == KM_DBL_CLICK) {
    /* EVT_DROP:
     *   If we're handling drop event we'll want it to be handled by popup callee as well,
     *   so it'll be possible to perform such operations as opening .blend files by dropping
     *   them into blender, even if there's opened popup like splash screen (sergey).
     * KM_DBL_CLICK:
     *   Continue in case of double click so wm_handlers_do calls handler again with KM_PRESS
     *   event. This is needed to ensure correct button handling for fast clicking (T47532).
     */

    retval = WM_UI_HANDLER_CONTINUE;
  }

  ui_handle_menus_recursive(C, event, menu, 0, false, false, true);

  /* free if done, does not free handle itself */
  if (menu->menuretval) {
    wmWindow *win = CTX_wm_window(C);
    /* copy values, we have to free first (closes region) */
    uiPopupBlockHandle temp = *menu;
    uiBlock *block = menu->region->uiblocks.first;

    /* set last pie event to allow chained pie spawning */
    if (block->flag & UI_BLOCK_RADIAL) {
      win->last_pie_event = block->pie_data.event;
      reset_pie = true;
    }

    ui_popup_block_free(C, menu);
    UI_popup_handlers_remove(&win->modalhandlers, menu);
    CTX_wm_menu_set(C, NULL);

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
      if (temp.optype) {
        WM_operator_name_call_ptr(C, temp.optype, temp.opcontext, NULL);
      }
    }
    else if (temp.cancel_func) {
      temp.cancel_func(C, temp.popup_arg);
    }

    WM_event_add_mousemove(C);
  }
  else {
    /* re-enable tooltips */
    if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy)) {
      ui_blocks_set_tooltips(menu->region, true);
    }
  }

  /* delayed apply callbacks */
  ui_apply_but_funcs_after(C);

  if (reset_pie) {
    /* reaqcuire window in case pie invalidates it somehow */
    wmWindow *win = CTX_wm_window(C);

    if (win) {
      win->last_pie_event = EVENT_NONE;
    }
  }

  CTX_wm_region_set(C, menu_region);

  return retval;
}

static void ui_popup_handler_remove(bContext *C, void *userdata)
{
  uiPopupBlockHandle *menu = userdata;

  /* More correct would be to expect UI_RETURN_CANCEL here, but not wanting to
   * cancel when removing handlers because of file exit is a rare exception.
   * So instead of setting cancel flag for all menus before removing handlers,
   * just explicitly flag menu with UI_RETURN_OK to avoid cancelling it. */
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
  WM_event_remove_ui_handler(handlers, ui_region_handler, ui_region_handler_remove, NULL, false);
  WM_event_add_ui_handler(NULL, handlers, ui_region_handler, ui_region_handler_remove, NULL, 0);
}

void UI_popup_handlers_add(bContext *C,
                           ListBase *handlers,
                           uiPopupBlockHandle *popup,
                           const char flag)
{
  WM_event_add_ui_handler(C, handlers, ui_popup_handler, ui_popup_handler_remove, popup, flag);
}

void UI_popup_handlers_remove(ListBase *handlers, uiPopupBlockHandle *popup)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;

      if (handler->handle_fn == ui_popup_handler &&
          handler->remove_fn == ui_popup_handler_remove && handler->user_data == popup) {
        /* tag refresh parent popup */
        wmEventHandler_UI *handler_next = (wmEventHandler_UI *)handler->head.next;
        if (handler_next && handler_next->head.type == WM_HANDLER_TYPE_UI &&
            handler_next->handle_fn == ui_popup_handler &&
            handler_next->remove_fn == ui_popup_handler_remove) {
          uiPopupBlockHandle *parent_popup = handler_next->user_data;
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
                                ARegion *ar,
                                const void *rna_poin_data,
                                const char *rna_prop_id)
{
  uiBlock *block;
  uiBut *but = NULL;

  for (block = ar->uiblocks.first; block; block = block->next) {
    for (but = block->buttons.first; but; but = but->next) {
      if (but->type == UI_BTYPE_TEXT) {
        if (but->rnaprop && but->rnapoin.data == rna_poin_data) {
          if (STREQ(RNA_property_identifier(but->rnaprop), rna_prop_id)) {
            break;
          }
        }
      }
    }
    if (but) {
      break;
    }
  }

  if (but) {
    UI_but_active_only(C, ar, block, but);
    return true;
  }
  else {
    return false;
  }
}

bool UI_textbutton_activate_but(const bContext *C, uiBut *actbut)
{
  ARegion *ar = CTX_wm_region(C);
  uiBlock *block;
  uiBut *but = NULL;

  for (block = ar->uiblocks.first; block; block = block->next) {
    for (but = block->buttons.first; but; but = but->next) {
      if (but == actbut && but->type == UI_BTYPE_TEXT) {
        break;
      }
    }

    if (but) {
      break;
    }
  }

  if (but) {
    UI_but_active_only(C, ar, block, but);
    return true;
  }
  else {
    return false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Utilities
 * \{ */

/* is called by notifier */
void UI_screen_free_active_but(const bContext *C, bScreen *screen)
{
  wmWindow *win = CTX_wm_window(C);

  ED_screen_areas_iter(win, screen, area)
  {
    for (ARegion *region = area->regionbase.first; region; region = region->next) {
      uiBut *but = ui_region_find_active_but(region);
      if (but) {
        uiHandleButtonData *data = but->active;

        if (data->menu == NULL && data->searchbox == NULL) {
          if (data->state == BUTTON_STATE_HIGHLIGHT) {
            ui_but_active_free(C, but);
          }
        }
      }
    }
  }
}

/* returns true if highlighted button allows drop of names */
/* called in region context */
bool UI_but_active_drop_name(bContext *C)
{
  ARegion *ar = CTX_wm_region(C);
  uiBut *but = ui_region_find_active_but(ar);

  if (but) {
    if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
      return 1;
    }
  }

  return 0;
}

bool UI_but_active_drop_color(bContext *C)
{
  ARegion *ar = CTX_wm_region(C);

  if (ar) {
    uiBut *but = ui_region_find_active_but(ar);

    if (but && but->type == UI_BTYPE_COLOR) {
      return true;
    }
  }

  return false;
}

/** \} */
