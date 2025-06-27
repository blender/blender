/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Read-only queries utility functions for the event system.
 */

#include <cstdlib>
#include <cstring>

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_event_system.hh"
#include "wm_event_types.hh"

#include "RNA_enum_types.hh"

/* -------------------------------------------------------------------- */
/** \name Event Printing
 * \{ */

struct FlagIdentifierPair {
  const char *id;
  uint flag;
};

static void event_ids_from_flag(char *str,
                                const int str_maxncpy,
                                const FlagIdentifierPair *flag_data,
                                const int flag_data_len,
                                const uint flag)
{
  int ofs = 0;
  ofs += BLI_strncpy_rlen(str + ofs, "{", str_maxncpy - ofs);
  for (int i = 0; i < flag_data_len; i++) {
    if (flag & flag_data[i].flag) {
      if (ofs != 1) {
        ofs += BLI_strncpy_rlen(str + ofs, "|", str_maxncpy - ofs);
      }
      ofs += BLI_strncpy_rlen(str + ofs, flag_data[i].id, str_maxncpy - ofs);
    }
  }
  ofs += BLI_strncpy_rlen(str + ofs, "}", str_maxncpy - ofs);
  UNUSED_VARS(ofs); /* Quiet warning. */
}

static void event_ids_from_type_and_value(const short type,
                                          const short val,
                                          const char **r_type_id,
                                          const char **r_val_id)
{
  /* Type. */
  RNA_enum_identifier(rna_enum_event_type_items, type, r_type_id);

  /* Value. */
  RNA_enum_identifier(rna_enum_event_value_items, val, r_val_id);
}

void WM_event_print(const wmEvent *event)
{
  if (event) {
    const char *unknown = "UNKNOWN";
    const char *type_id = unknown;
    const char *val_id = unknown;
    const char *prev_type_id = unknown;
    const char *prev_val_id = unknown;

    event_ids_from_type_and_value(event->type, event->val, &type_id, &val_id);
    event_ids_from_type_and_value(event->prev_type, event->prev_val, &prev_type_id, &prev_val_id);

    char modifier_id[128];
    {
      FlagIdentifierPair flag_data[] = {
          {"SHIFT", KM_SHIFT},
          {"CTRL", KM_CTRL},
          {"ALT", KM_ALT},
          {"OS", KM_OSKEY},
          {"HYPER", KM_HYPER},

      };
      event_ids_from_flag(
          modifier_id, sizeof(modifier_id), flag_data, ARRAY_SIZE(flag_data), event->modifier);
    }

    char flag_id[128];
    {
      FlagIdentifierPair flag_data[] = {
          {"SCROLL_INVERT", WM_EVENT_SCROLL_INVERT},
          {"IS_REPEAT", WM_EVENT_IS_REPEAT},
          {"IS_CONSECUTIVE", WM_EVENT_IS_CONSECUTIVE},
          {"FORCE_DRAG_THRESHOLD", WM_EVENT_FORCE_DRAG_THRESHOLD},
      };
      event_ids_from_flag(flag_id, sizeof(flag_id), flag_data, ARRAY_SIZE(flag_data), event->flag);
    }

    printf(
        "wmEvent type:%d/%s, val:%d/%s, "
        "prev_type:%d/%s, prev_val:%d/%s, "
        "modifier=%s, keymodifier:%d, flag:%s, "
        "mouse:(%d,%d), utf8:'%.*s', pointer:%p",
        event->type,
        type_id,
        event->val,
        val_id,
        event->prev_type,
        prev_type_id,
        event->prev_val,
        prev_val_id,
        modifier_id,
        event->keymodifier,
        flag_id,
        event->xy[0],
        event->xy[1],
        BLI_str_utf8_size_or_error(event->utf8_buf),
        event->utf8_buf,
        (const void *)event);

#ifdef WITH_INPUT_NDOF
    if (ISNDOF(event->type)) {
      const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);
      if (event->type == NDOF_MOTION) {
        const char *ndof_progress = unknown;

#  define CASE_NDOF_PROGRESS(id) \
    case P_##id: { \
      ndof_progress = STRINGIFY(id); \
      break; \
    }
        switch (ndof.progress) {
          CASE_NDOF_PROGRESS(NOT_STARTED);
          CASE_NDOF_PROGRESS(STARTING);
          CASE_NDOF_PROGRESS(IN_PROGRESS);
          CASE_NDOF_PROGRESS(FINISHING);
          CASE_NDOF_PROGRESS(FINISHED);
        }
#  undef CASE_NDOF_PROGRESS

        printf(
            ", ndof: "
            "rot: (%.4f %.4f %.4f), "
            "tx: (%.4f %.4f %.4f), "
            "time_delta: %.4f, "
            "progress: %s",
            UNPACK3(ndof.rvec),
            UNPACK3(ndof.tvec),
            ndof.time_delta,
            ndof_progress);
      }
      else {
        /* NDOF buttons printed already. */
      }
    }
#endif /* WITH_INPUT_NDOF */

    if (event->tablet.active != EVT_TABLET_NONE) {
      const wmTabletData *wmtab = &event->tablet;
      printf(", tablet: active: %d, pressure %.4f, tilt: (%.4f %.4f)",
             wmtab->active,
             wmtab->pressure,
             wmtab->tilt.x,
             wmtab->tilt.y);
    }
    printf("\n");
  }
  else {
    printf("wmEvent - nullptr\n");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Modifier/Type Queries
 * \{ */

bool WM_event_type_mask_test(const int event_type, const enum eEventType_Mask mask)
{
  /* Keyboard. */
  if (mask & EVT_TYPE_MASK_KEYBOARD) {
    if (ISKEYBOARD(event_type)) {
      return true;
    }
  }
  else if (mask & EVT_TYPE_MASK_KEYBOARD_MODIFIER) {
    if (ISKEYMODIFIER(event_type)) {
      return true;
    }
  }

  /* Mouse. */
  if (mask & EVT_TYPE_MASK_MOUSE) {
    if (ISMOUSE(event_type)) {
      return true;
    }
  }
  else if (mask & EVT_TYPE_MASK_MOUSE_WHEEL) {
    if (ISMOUSE_WHEEL(event_type)) {
      return true;
    }
  }
  else if (mask & EVT_TYPE_MASK_MOUSE_GESTURE) {
    if (ISMOUSE_GESTURE(event_type)) {
      return true;
    }
  }

  /* NDOF. */
  if (mask & EVT_TYPE_MASK_NDOF) {
    if (ISNDOF(event_type)) {
      return true;
    }
  }

  /* Action Zone. */
  if (mask & EVT_TYPE_MASK_ACTIONZONE) {
    if (IS_EVENT_ACTIONZONE(event_type)) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Motion Queries
 * \{ */

bool WM_event_is_modal_drag_exit(const wmEvent *event,
                                 const short init_event_type,
                                 const short init_event_val)
{
  /* If the release-confirm preference setting is enabled,
   * drag events can be canceled when mouse is released. */
  if (U.flag & USER_RELEASECONFIRM) {
    /* Option on, so can exit with km-release. */
    if (event->val == KM_RELEASE) {
      if ((init_event_val == KM_CLICK_DRAG) && (event->type == init_event_type)) {
        return true;
      }
    }
    else {
      /* If the initial event wasn't a drag event then
       * ignore #USER_RELEASECONFIRM setting: see #26756. */
      if (init_event_val != KM_CLICK_DRAG) {
        return true;
      }
    }
  }
  else {
    /* This is fine as long as not doing km-release, otherwise some items (i.e. markers)
     * being tweaked may end up getting dropped all over. */
    if (event->val != KM_RELEASE) {
      return true;
    }
  }

  return false;
}

bool WM_event_is_mouse_drag(const wmEvent *event)
{
  return (ISMOUSE_BUTTON(event->type) && (event->val == KM_CLICK_DRAG));
}

bool WM_event_is_mouse_drag_or_press(const wmEvent *event)
{
  return WM_event_is_mouse_drag(event) ||
         (ISMOUSE_BUTTON(event->type) && (event->val == KM_PRESS));
}

int WM_event_drag_direction(const wmEvent *event)
{
  const int delta[2] = {
      event->xy[0] - event->prev_press_xy[0],
      event->xy[1] - event->prev_press_xy[1],
  };

  int theta = round_fl_to_int(4.0f * atan2f(float(delta[1]), float(delta[0])) / float(M_PI));
  int val = KM_DIRECTION_W;

  if (theta == 0) {
    val = KM_DIRECTION_E;
  }
  else if (theta == 1) {
    val = KM_DIRECTION_NE;
  }
  else if (theta == 2) {
    val = KM_DIRECTION_N;
  }
  else if (theta == 3) {
    val = KM_DIRECTION_NW;
  }
  else if (theta == -1) {
    val = KM_DIRECTION_SE;
  }
  else if (theta == -2) {
    val = KM_DIRECTION_S;
  }
  else if (theta == -3) {
    val = KM_DIRECTION_SW;
  }

#if 0
  /* Debug. */
  if (val == 1) {
    printf("tweak north\n");
  }
  if (val == 2) {
    printf("tweak north-east\n");
  }
  if (val == 3) {
    printf("tweak east\n");
  }
  if (val == 4) {
    printf("tweak south-east\n");
  }
  if (val == 5) {
    printf("tweak south\n");
  }
  if (val == 6) {
    printf("tweak south-west\n");
  }
  if (val == 7) {
    printf("tweak west\n");
  }
  if (val == 8) {
    printf("tweak north-west\n");
  }
#endif
  return val;
}

bool WM_cursor_test_motion_and_update(const int mval[2])
{
  static int mval_prev[2] = {-1, -1};
  bool use_cycle = (len_manhattan_v2v2_int(mval, mval_prev) <= WM_EVENT_CURSOR_MOTION_THRESHOLD);
  copy_v2_v2_int(mval_prev, mval);
  return !use_cycle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Consecutive Checks
 * \{ */

bool WM_event_consecutive_gesture_test(const wmEvent *event)
{
  return ISMOUSE_GESTURE(event->type) || (event->type == NDOF_MOTION);
}

bool WM_event_consecutive_gesture_test_break(const wmWindow *win, const wmEvent *event)
{
  /* Cursor motion breaks the chain. */
  if (ISMOUSE_MOTION(event->type)) {
    /* Mouse motion is checked because the user may navigate to a new area
     * and perform the same gesture - logically it's best to view this as two separate gestures. */
    if (len_manhattan_v2v2_int(event->xy, win->event_queue_consecutive_gesture_xy) >
        WM_EVENT_CURSOR_MOTION_THRESHOLD)
    {
      return true;
    }
  }
  else if (ISKEYBOARD_OR_BUTTON(event->type)) {
    /* Modifiers are excluded because from a user perspective.
     * For example, releasing a modifier should not begin a new action. */
    if (!ISKEYMODIFIER(event->type)) {
      return true;
    }
  }
  else if (event->type == WINDEACTIVATE) {
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Click/Drag Checks
 *
 * Values under this limit are detected as clicks.
 *
 * \{ */

int WM_event_drag_threshold(const wmEvent *event)
{
  int drag_threshold;
  BLI_assert(event->prev_press_type != MOUSEMOVE);
  if (ISMOUSE_BUTTON(event->prev_press_type)) {
    /* Using the previous type is important is we want to check the last pressed/released button,
     * The `event->type` would include #MOUSEMOVE which is always the case when dragging
     * and does not help us know which threshold to use. */
    if (WM_event_is_tablet(event)) {
      drag_threshold = U.drag_threshold_tablet;
    }
    else {
      drag_threshold = U.drag_threshold_mouse;
    }
  }
  else {
    /* Typically keyboard, could be NDOF button or other less common types. */
    drag_threshold = U.drag_threshold;
  }
  return drag_threshold * UI_SCALE_FAC;
}

bool WM_event_drag_test_with_delta(const wmEvent *event, const int drag_delta[2])
{
  const int drag_threshold = WM_event_drag_threshold(event);
  return abs(drag_delta[0]) > drag_threshold || abs(drag_delta[1]) > drag_threshold;
}

bool WM_event_drag_test(const wmEvent *event, const int prev_xy[2])
{
  int drag_delta[2];
  sub_v2_v2v2_int(drag_delta, prev_xy, event->xy);
  return WM_event_drag_test_with_delta(event, drag_delta);
}

void WM_event_drag_start_mval(const wmEvent *event, const ARegion *region, int r_mval[2])
{
  const int *xy = (event->val == KM_CLICK_DRAG) ? event->prev_press_xy : event->xy;
  r_mval[0] = xy[0] - region->winrct.xmin;
  r_mval[1] = xy[1] - region->winrct.ymin;
}

void WM_event_drag_start_mval_fl(const wmEvent *event, const ARegion *region, float r_mval[2])
{
  const int *xy = (event->val == KM_CLICK_DRAG) ? event->prev_press_xy : event->xy;
  r_mval[0] = xy[0] - region->winrct.xmin;
  r_mval[1] = xy[1] - region->winrct.ymin;
}

void WM_event_drag_start_xy(const wmEvent *event, int r_xy[2])
{
  copy_v2_v2_int(r_xy, (event->val == KM_CLICK_DRAG) ? event->prev_press_xy : event->xy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Text Queries
 * \{ */

char WM_event_utf8_to_ascii(const wmEvent *event)
{
  if (BLI_str_utf8_size_or_error(event->utf8_buf) == 1) {
    return event->utf8_buf[0];
  }
  return '\0';
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Preference Mapping
 * \{ */

int WM_userdef_event_map(int kmitype)
{
  switch (kmitype) {
    case WHEELOUTMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELUPMOUSE : WHEELDOWNMOUSE;
    case WHEELINMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELDOWNMOUSE : WHEELUPMOUSE;
  }

  return kmitype;
}

int WM_userdef_event_type_from_keymap_type(int kmitype)
{
  switch (kmitype) {
    case WHEELOUTMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELUPMOUSE : WHEELDOWNMOUSE;
    case WHEELINMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELDOWNMOUSE : WHEELUPMOUSE;
  }

  return kmitype;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event NDOF Input Access
 * \{ */

#ifdef WITH_INPUT_NDOF

static blender::float3 event_ndof_translation_get_with_sign(const wmNDOFMotionData &ndof,
                                                            const float sign)
{
  int ndof_flag = U.ndof_flag;
  int x = 0, y = 1, z = 2;
  if (ndof_flag & NDOF_SWAP_YZ_AXIS) {
    /* Map `{x, y, z}` -> `{x, -z, y}`. */
    std::swap(y, z);
    ndof_flag ^= NDOF_PANY_INVERT_AXIS;
  }
  return {
      ndof.tvec[x] * ((ndof_flag & NDOF_PANX_INVERT_AXIS) ? -sign : sign),
      ndof.tvec[y] * ((ndof_flag & NDOF_PANY_INVERT_AXIS) ? -sign : sign),
      ndof.tvec[z] * ((ndof_flag & NDOF_PANZ_INVERT_AXIS) ? -sign : sign),
  };
}

static blender::float3 event_ndof_rotation_get_with_sign(const wmNDOFMotionData &ndof,
                                                         const float sign)
{
  int ndof_flag = U.ndof_flag;
  int x = 0, y = 1, z = 2;
  if (ndof_flag & NDOF_SWAP_YZ_AXIS) {
    /* Map `{x, y, z}` -> `{x, -z, y}`. */
    std::swap(y, z);
    ndof_flag ^= NDOF_ROTY_INVERT_AXIS;
  }
  return {
      ndof.rvec[x] * ((ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -sign : sign),
      ndof.rvec[y] * ((ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -sign : sign),
      ndof.rvec[z] * ((ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -sign : sign),
  };
}

blender::float3 WM_event_ndof_translation_get_for_navigation(const wmNDOFMotionData &ndof)
{
  const float sign = (U.ndof_navigation_mode == NDOF_NAVIGATION_MODE_OBJECT) ? -1.0f : 1.0f;
  return event_ndof_translation_get_with_sign(ndof, sign);
}

blender::float3 WM_event_ndof_rotation_get_for_navigation(const wmNDOFMotionData &ndof)
{
  const float sign = (U.ndof_navigation_mode == NDOF_NAVIGATION_MODE_OBJECT) ? -1.0f : 1.0f;
  return event_ndof_rotation_get_with_sign(ndof, sign);
}

blender::float3 WM_event_ndof_translation_get(const wmNDOFMotionData &ndof)
{
  return event_ndof_translation_get_with_sign(ndof, 1.0f);
}

blender::float3 WM_event_ndof_rotation_get(const wmNDOFMotionData &ndof)
{
  return event_ndof_rotation_get_with_sign(ndof, 1.0f);
}

float WM_event_ndof_rotation_get_axis_angle_for_navigation(const wmNDOFMotionData &ndof,
                                                           float axis[3])
{
  const blender::float3 rvec = WM_event_ndof_rotation_get_for_navigation(ndof);
  return normalize_v3_v3(axis, rvec);
}

float WM_event_ndof_rotation_get_axis_angle(const wmNDOFMotionData &ndof, float axis[3])
{
  const blender::float3 rvec = WM_event_ndof_rotation_get(ndof);
  return normalize_v3_v3(axis, rvec);
}

bool WM_event_ndof_translation_has_pan(const wmNDOFMotionData &ndof)
{
  return (U.ndof_flag & NDOF_SWAP_YZ_AXIS) ? ((ndof.tvec[0] != 0.0f) || (ndof.tvec[2] != 0.0f)) :
                                             ((ndof.tvec[0] != 0.0f) || (ndof.tvec[1] != 0.0f));
}

bool WM_event_ndof_translation_has_zoom(const wmNDOFMotionData &ndof)
{
  return ndof.tvec[(U.ndof_flag & NDOF_SWAP_YZ_AXIS) ? 1 : 2] != 0.0f;
}

#endif /* WITH_INPUT_NDOF */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event XR Input Access
 * \{ */

#ifdef WITH_XR_OPENXR
bool WM_event_is_xr(const wmEvent *event)
{
  return (event->type == EVT_XR_ACTION && event->custom == EVT_DATA_XR);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Tablet Input Access
 * \{ */

float wm_pressure_curve(float raw_pressure)
{
  if (U.pressure_threshold_max != 0.0f) {
    raw_pressure /= U.pressure_threshold_max;
  }

  CLAMP(raw_pressure, 0.0f, 1.0f);

  if (U.pressure_softness != 0.0f) {
    raw_pressure = powf(raw_pressure, powf(4.0f, -U.pressure_softness));
  }

  return raw_pressure;
}

float WM_event_tablet_data(const wmEvent *event, bool *r_pen_flip, float r_tilt[2])
{
  if (r_tilt) {
    copy_v2_v2(r_tilt, event->tablet.tilt);
  }

  if (r_pen_flip) {
    (*r_pen_flip) = (event->tablet.active == EVT_TABLET_ERASER);
  }

  return event->tablet.pressure;
}

bool WM_event_is_tablet(const wmEvent *event)
{
  return (event->tablet.active != EVT_TABLET_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Scroll's Absolute Deltas
 *
 * User may change the scroll behavior, and the deltas are automatically inverted.
 * These functions return the absolute direction, swipe up/right gives positive values.
 *
 * \{ */

int WM_event_absolute_delta_x(const wmEvent *event)
{
  int dx = event->xy[0] - event->prev_xy[0];

  if ((event->flag & WM_EVENT_SCROLL_INVERT) == 0) {
    dx = -dx;
  }

  return dx;
}

int WM_event_absolute_delta_y(const wmEvent *event)
{
  int dy = event->xy[1] - event->prev_xy[1];

  if ((event->flag & WM_EVENT_SCROLL_INVERT) == 0) {
    dy = -dy;
  }

  return dy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event IME Input Access
 * \{ */

#ifdef WITH_INPUT_IME
bool WM_event_is_ime_switch(const wmEvent *event)
{
  /* Most OS's use `Ctrl+Space` / `OsKey+Space` to switch IME,
   * so don't type in the space character.
   *
   * NOTE: Shift is excluded from this check since it prevented typing `Shift+Space`, see: #85517.
   */
  return (event->val == KM_PRESS) && (event->type == EVT_SPACEKEY) &&
         (event->modifier & (KM_CTRL | KM_OSKEY | KM_ALT));
}
#endif

/** \} */
