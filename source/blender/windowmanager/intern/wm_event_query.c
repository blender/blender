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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Read-only queries utility functions for the event system.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_event_system.h"
#include "wm_event_types.h"

#include "RNA_enum_types.h"

#include "DEG_depsgraph.h"

/* -------------------------------------------------------------------- */
/** \name Event Printing
 * \{ */

static void event_ids_from_type_and_value(const short type,
                                          const short val,
                                          const char **r_type_id,
                                          const char **r_val_id)
{
  /* Type. */
  RNA_enum_identifier(rna_enum_event_type_items, type, r_type_id);

  /* Value. */
  if (ISTWEAK(type)) {
    RNA_enum_identifier(rna_enum_event_value_tweak_items, val, r_val_id);
  }
  else {
    RNA_enum_identifier(rna_enum_event_value_all_items, val, r_val_id);
  }
}

/* for debugging only, getting inspecting events manually is tedious */
void WM_event_print(const wmEvent *event)
{
  if (event) {
    const char *unknown = "UNKNOWN";
    const char *type_id = unknown;
    const char *val_id = unknown;
    const char *prev_type_id = unknown;
    const char *prev_val_id = unknown;

    event_ids_from_type_and_value(event->type, event->val, &type_id, &val_id);
    event_ids_from_type_and_value(event->prevtype, event->prevval, &prev_type_id, &prev_val_id);

    printf(
        "wmEvent  type:%d / %s, val:%d / %s,\n"
        "         prev_type:%d / %s, prev_val:%d / %s,\n"
        "         shift:%d, ctrl:%d, alt:%d, oskey:%d, keymodifier:%d, is_repeat:%d,\n"
        "         mouse:(%d,%d), ascii:'%c', utf8:'%.*s', pointer:%p\n",
        event->type,
        type_id,
        event->val,
        val_id,
        event->prevtype,
        prev_type_id,
        event->prevval,
        prev_val_id,
        event->shift,
        event->ctrl,
        event->alt,
        event->oskey,
        event->keymodifier,
        event->is_repeat,
        event->x,
        event->y,
        event->ascii,
        BLI_str_utf8_size(event->utf8_buf),
        event->utf8_buf,
        (const void *)event);

#ifdef WITH_INPUT_NDOF
    if (ISNDOF(event->type)) {
      const wmNDOFMotionData *ndof = event->customdata;
      if (event->type == NDOF_MOTION) {
        printf("   ndof: rot: (%.4f %.4f %.4f), tx: (%.4f %.4f %.4f), dt: %.4f, progress: %u\n",
               UNPACK3(ndof->rvec),
               UNPACK3(ndof->tvec),
               ndof->dt,
               ndof->progress);
      }
      else {
        /* ndof buttons printed already */
      }
    }
#endif /* WITH_INPUT_NDOF */

    if (event->tablet.active != EVT_TABLET_NONE) {
      const wmTabletData *wmtab = &event->tablet;
      printf(" tablet: active: %d, pressure %.4f, tilt: (%.4f %.4f)\n",
             wmtab->active,
             wmtab->pressure,
             wmtab->x_tilt,
             wmtab->y_tilt);
    }
  }
  else {
    printf("wmEvent - NULL\n");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Modifier/Type Queries
 * \{ */

int WM_event_modifier_flag(const wmEvent *event)
{
  int flag = 0;
  if (event->ctrl) {
    flag |= KM_CTRL;
  }
  if (event->alt) {
    flag |= KM_ALT;
  }
  if (event->shift) {
    flag |= KM_SHIFT;
  }
  if (event->oskey) {
    flag |= KM_OSKEY;
  }
  return flag;
}

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

  /* Tweak. */
  if (mask & EVT_TYPE_MASK_TWEAK) {
    if (ISTWEAK(event_type)) {
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

/* for modal callbacks, check configuration for how to interpret exit with tweaks  */
bool WM_event_is_modal_tweak_exit(const wmEvent *event, int tweak_event)
{
  /* if the release-confirm userpref setting is enabled,
   * tweak events can be canceled when mouse is released
   */
  if (U.flag & USER_RELEASECONFIRM) {
    /* option on, so can exit with km-release */
    if (event->val == KM_RELEASE) {
      switch (tweak_event) {
        case EVT_TWEAK_L:
        case EVT_TWEAK_M:
        case EVT_TWEAK_R:
          return 1;
      }
    }
    else {
      /* if the initial event wasn't a tweak event then
       * ignore USER_RELEASECONFIRM setting: see T26756. */
      if (ELEM(tweak_event, EVT_TWEAK_L, EVT_TWEAK_M, EVT_TWEAK_R) == 0) {
        return 1;
      }
    }
  }
  else {
    /* this is fine as long as not doing km-release, otherwise
     * some items (i.e. markers) being tweaked may end up getting
     * dropped all over
     */
    if (event->val != KM_RELEASE) {
      return 1;
    }
  }

  return 0;
}

bool WM_event_is_last_mousemove(const wmEvent *event)
{
  while ((event = event->next)) {
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
      return false;
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Click/Drag Checks
 *
 * Values under this limit are detected as clicks.
 *
 * \{ */

int WM_event_drag_threshold(const struct wmEvent *event)
{
  int drag_threshold;
  if (WM_event_is_tablet(event)) {
    drag_threshold = U.drag_threshold_tablet;
  }
  else if (ISMOUSE(event->prevtype)) {
    drag_threshold = U.drag_threshold_mouse;
  }
  else {
    /* Typically keyboard, could be NDOF button or other less common types. */
    drag_threshold = U.drag_threshold;
  }
  return drag_threshold * U.dpi_fac;
}

bool WM_event_drag_test_with_delta(const wmEvent *event, const int drag_delta[2])
{
  const int drag_threshold = WM_event_drag_threshold(event);
  return abs(drag_delta[0]) > drag_threshold || abs(drag_delta[1]) > drag_threshold;
}

bool WM_event_drag_test(const wmEvent *event, const int prev_xy[2])
{
  const int drag_delta[2] = {
      prev_xy[0] - event->x,
      prev_xy[1] - event->y,
  };
  return WM_event_drag_test_with_delta(event, drag_delta);
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

/**
 * Use so we can check if 'wmEvent.type' is released in modal operators.
 *
 * An alternative would be to add a 'wmEvent.type_nokeymap'... or similar.
 */
int WM_userdef_event_type_from_keymap_type(int kmitype)
{
  switch (kmitype) {
    case EVT_TWEAK_L:
      return LEFTMOUSE;
    case EVT_TWEAK_M:
      return MIDDLEMOUSE;
    case EVT_TWEAK_R:
      return RIGHTMOUSE;
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

void WM_event_ndof_pan_get(const wmNDOFMotionData *ndof, float r_pan[3], const bool use_zoom)
{
  int z_flag = use_zoom ? NDOF_ZOOM_INVERT : NDOF_PANZ_INVERT_AXIS;
  r_pan[0] = ndof->tvec[0] * ((U.ndof_flag & NDOF_PANX_INVERT_AXIS) ? -1.0f : 1.0f);
  r_pan[1] = ndof->tvec[1] * ((U.ndof_flag & NDOF_PANY_INVERT_AXIS) ? -1.0f : 1.0f);
  r_pan[2] = ndof->tvec[2] * ((U.ndof_flag & z_flag) ? -1.0f : 1.0f);
}

void WM_event_ndof_rotate_get(const wmNDOFMotionData *ndof, float r_rot[3])
{
  r_rot[0] = ndof->rvec[0] * ((U.ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -1.0f : 1.0f);
  r_rot[1] = ndof->rvec[1] * ((U.ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -1.0f : 1.0f);
  r_rot[2] = ndof->rvec[2] * ((U.ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -1.0f : 1.0f);
}

float WM_event_ndof_to_axis_angle(const struct wmNDOFMotionData *ndof, float axis[3])
{
  float angle;
  angle = normalize_v3_v3(axis, ndof->rvec);

  axis[0] = axis[0] * ((U.ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -1.0f : 1.0f);
  axis[1] = axis[1] * ((U.ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -1.0f : 1.0f);
  axis[2] = axis[2] * ((U.ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -1.0f : 1.0f);

  return ndof->dt * angle;
}

void WM_event_ndof_to_quat(const struct wmNDOFMotionData *ndof, float q[4])
{
  float axis[3];
  float angle;

  angle = WM_event_ndof_to_axis_angle(ndof, axis);
  axis_angle_to_quat(q, axis, angle);
}
#endif /* WITH_INPUT_NDOF */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Tablet Input Access
 * \{ */

/* applies the global tablet pressure correction curve */
float wm_pressure_curve(float pressure)
{
  if (U.pressure_threshold_max != 0.0f) {
    pressure /= U.pressure_threshold_max;
  }

  CLAMP(pressure, 0.0f, 1.0f);

  if (U.pressure_softness != 0.0f) {
    pressure = powf(pressure, powf(4.0f, -U.pressure_softness));
  }

  return pressure;
}

/* if this is a tablet event, return tablet pressure and set *pen_flip
 * to 1 if the eraser tool is being used, 0 otherwise */
float WM_event_tablet_data(const wmEvent *event, int *pen_flip, float tilt[2])
{
  if (tilt) {
    tilt[0] = event->tablet.x_tilt;
    tilt[1] = event->tablet.y_tilt;
  }

  if (pen_flip) {
    (*pen_flip) = (event->tablet.active == EVT_TABLET_ERASER);
  }

  return event->tablet.pressure;
}

bool WM_event_is_tablet(const struct wmEvent *event)
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

int WM_event_absolute_delta_x(const struct wmEvent *event)
{
  int dx = event->x - event->prevx;

  if (!event->is_direction_inverted) {
    dx = -dx;
  }

  return dx;
}

int WM_event_absolute_delta_y(const struct wmEvent *event)
{
  int dy = event->y - event->prevy;

  if (!event->is_direction_inverted) {
    dy = -dy;
  }

  return dy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event IME Input Access
 * \{ */

#ifdef WITH_INPUT_IME
/* most os using ctrl/oskey + space to switch ime, avoid added space */
bool WM_event_is_ime_switch(const struct wmEvent *event)
{
  return event->val == KM_PRESS && event->type == EVT_SPACEKEY &&
         (event->ctrl || event->oskey || event->shift || event->alt);
}
#endif

/** \} */
