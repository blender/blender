/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_view3d_types.h"

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

  /** Object type settings to apply to VR view (unlike shading, not shared with window 3D-View). */
  int object_type_exclude_viewport;
  int object_type_exclude_select;

  /** Fly speed. */
  float fly_speed;
  float padding;
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
 * For axis-based inputs (thumbstick/trackpad/etc).
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

/**
 * The following user and component path lengths are dependent on OpenXR's XR_MAX_PATH_LENGTH
 * (256). A user path will be combined with a component path to identify an action binding, and
 * that combined path should also have a max of XR_MAX_PATH_LENGTH (e.g. user_path =
 * /user/hand/left, component_path = /input/trigger/value, full_path =
 * /user/hand/left/input/trigger/value).
 */
#define XR_MAX_USER_PATH_LENGTH 64
#define XR_MAX_COMPONENT_PATH_LENGTH 192

/* -------------------------------------------------------------------- */

typedef struct XrComponentPath {
  struct XrComponentPath *next, *prev;
  char path[/*XR_MAX_COMPONENT_PATH_LENGTH*/ 192];
} XrComponentPath;

typedef struct XrActionMapBinding {
  struct XrActionMapBinding *next, *prev;

  /** Unique name. */
  char name[/*MAX_NAME*/ 64];

  /** OpenXR interaction profile path. */
  char profile[256];
  /** OpenXR component paths. */
  ListBase component_paths; /* XrComponentPath */

  /** Input threshold/region. */
  float float_threshold;
  short axis_flag; /* eXrAxisFlag */
  char _pad[2];

  /** Pose action properties. */
  float pose_location[3];
  float pose_rotation[3];
} XrActionMapBinding;

/* -------------------------------------------------------------------- */

typedef struct XrUserPath {
  struct XrUserPath *next, *prev;
  char path[/*XR_MAX_USER_PATH_LENGTH*/ 64];
} XrUserPath;

typedef struct XrActionMapItem {
  struct XrActionMapItem *next, *prev;

  /** Unique name. */
  char name[/*MAX_NAME*/ 64];
  /** Type. */
  char type; /** eXrActionType */
  char _pad[7];

  /** OpenXR user paths. */
  ListBase user_paths; /* XrUserPath */

  /** Operator to be called on XR events. */
  char op[/*OP_MAX_TYPENAME*/ 64];
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
  char haptic_name[/*MAX_NAME*/ 64];
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
  char name[/*MAX_NAME*/ 64];

  ListBase items; /* XrActionMapItem */
  short selitem;
  char _pad[6];
} XrActionMap;

/* -------------------------------------------------------------------- */
