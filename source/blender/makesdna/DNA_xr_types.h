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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_view3d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */

typedef struct XrSessionSettings {
  /** Shading settings, struct shared with 3D-View so settings are the same. */
  struct View3DShading shading;

  float base_scale;
  char _pad[3];
  char base_pose_type; /* #eXRSessionBasePoseType */
  /** Object to take the location and rotation as base position from. */
  Object *base_pose_object;
  float base_pose_location[3];
  float base_pose_angle;

  /** View3D draw flags (V3D_OFSDRAW_NONE, V3D_OFSDRAW_SHOW_ANNOTATION, ...). */
  char draw_flags;
  /** Draw style for controller visualization. */
  char controller_draw_style;
  char _pad2[2];

  /** Clipping distance. */
  float clip_start, clip_end;

  int flag;
} XrSessionSettings;

typedef enum eXrSessionFlag {
  XR_SESSION_USE_POSITION_TRACKING = (1 << 0),
  XR_SESSION_USE_ABSOLUTE_TRACKING = (1 << 1),
} eXrSessionFlag;

typedef enum eXRSessionBasePoseType {
  XR_BASE_POSE_SCENE_CAMERA = 0,
  XR_BASE_POSE_OBJECT = 1,
  XR_BASE_POSE_CUSTOM = 2,
} eXRSessionBasePoseType;

typedef enum eXrSessionControllerDrawStyle {
  XR_CONTROLLER_DRAW_DARK = 0,
  XR_CONTROLLER_DRAW_LIGHT = 1,
  XR_CONTROLLER_DRAW_DARK_RAY = 2,
  XR_CONTROLLER_DRAW_LIGHT_RAY = 3,
} eXrSessionControllerDrawStyle;

/** XR action type. Enum values match those in GHOST_XrActionType enum for consistency. */
typedef enum eXrActionType {
  XR_BOOLEAN_INPUT = 1,
  XR_FLOAT_INPUT = 2,
  XR_VECTOR2F_INPUT = 3,
  XR_POSE_INPUT = 4,
  XR_VIBRATION_OUTPUT = 100,
} eXrActionType;

/** Determines how XR action operators are executed. */
typedef enum eXrOpFlag {
  XR_OP_PRESS = 0,
  XR_OP_RELEASE = 1,
  XR_OP_MODAL = 2,
} eXrOpFlag;

typedef enum eXrActionFlag {
  /** Action depends on two sub-action paths (i.e. two-handed/bi-manual action). */
  XR_ACTION_BIMANUAL = (1 << 0),
} eXrActionFlag;

typedef enum eXrHapticFlag {
  /** Whether to apply haptics to corresponding user paths for an action and its haptic action. */
  XR_HAPTIC_MATCHUSERPATHS = (1 << 0),
  /**
   * Determines how haptics will be applied
   * ("repeat" is mutually exclusive with "press"/"release").
   */
  XR_HAPTIC_PRESS = (1 << 1),
  XR_HAPTIC_RELEASE = (1 << 2),
  XR_HAPTIC_REPEAT = (1 << 3),
} eXrHapticFlag;

/**
 * For axis-based inputs (thumb-stick/track-pad/etc).
 * Determines the region for action execution (mutually exclusive per axis).
 */
typedef enum eXrAxisFlag {
  XR_AXIS0_POS = (1 << 0),
  XR_AXIS0_NEG = (1 << 1),
  XR_AXIS1_POS = (1 << 2),
  XR_AXIS1_NEG = (1 << 3),
} eXrAxisFlag;

typedef enum eXrPoseFlag {
  /* Pose represents controller grip/aim. */
  XR_POSE_GRIP = (1 << 0),
  XR_POSE_AIM = (1 << 1),
} eXrPoseFlag;

/* -------------------------------------------------------------------- */

typedef struct XrActionMapBinding {
  struct XrActionMapBinding *next, *prev;

  /** Unique name. */
  char name[64]; /* MAX_NAME */

  /** OpenXR interaction profile path. */
  char profile[256];
  /** OpenXR component paths. */
  char component_path0[192];
  char component_path1[192];

  /** Input threshold/region. */
  float float_threshold;
  short axis_flag; /* eXrAxisFlag */
  char _pad[2];

  /** Pose action properties. */
  float pose_location[3];
  float pose_rotation[3];
} XrActionMapBinding;

/* -------------------------------------------------------------------- */

typedef struct XrActionMapItem {
  struct XrActionMapItem *next, *prev;

  /** Unique name. */
  char name[64]; /* MAX_NAME */
  /** Type. */
  char type; /** eXrActionType */
  char _pad[7];

  /** OpenXR user paths. */
  char user_path0[64];
  char user_path1[64];

  /** Operator to be called on XR events. */
  char op[64]; /* OP_MAX_TYPENAME */
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  IDProperty *op_properties;
  /** RNA pointer to access properties. */
  struct PointerRNA *op_properties_ptr;

  short op_flag;     /* eXrOpFlag */
  short action_flag; /* eXrActionFlag */
  short haptic_flag; /* eXrHapticFlag */

  /** Pose action properties. */
  short pose_flag; /* eXrPoseFlag */

  /** Haptic properties. */
  char haptic_name[64]; /* MAX_NAME */
  float haptic_duration;
  float haptic_frequency;
  float haptic_amplitude;

  short selbinding;
  char _pad3[2];
  ListBase bindings; /* XrActionMapBinding */
} XrActionMapItem;

/* -------------------------------------------------------------------- */

typedef struct XrActionMap {
  struct XrActionMap *next, *prev;

  /** Unique name. */
  char name[64]; /* MAX_NAME */

  ListBase items; /* XrActionMapItem */
  short selitem;
  char _pad[6];
} XrActionMap;

/* -------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
