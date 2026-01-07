/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_view3d_types.h"

namespace blender {

enum eXrSessionFlag {
  XR_SESSION_USE_POSITION_TRACKING = (1 << 0),
  XR_SESSION_USE_ABSOLUTE_TRACKING = (1 << 1),
};

enum eXRSessionBasePoseType {
  XR_BASE_POSE_SCENE_CAMERA = 0,
  XR_BASE_POSE_OBJECT = 1,
  XR_BASE_POSE_CUSTOM = 2,
};

enum eXrSessionControllerDrawStyle {
  XR_CONTROLLER_DRAW_DARK = 0,
  XR_CONTROLLER_DRAW_LIGHT = 1,
  XR_CONTROLLER_DRAW_DARK_RAY = 2,
  XR_CONTROLLER_DRAW_LIGHT_RAY = 3,
};

/** XR action type. Enum values match those in GHOST_XrActionType enum for consistency. */
enum eXrActionType {
  XR_BOOLEAN_INPUT = 1,
  XR_FLOAT_INPUT = 2,
  XR_VECTOR2F_INPUT = 3,
  XR_POSE_INPUT = 4,
  XR_VIBRATION_OUTPUT = 100,
};

/** Determines how XR action operators are executed. */
enum eXrOpFlag {
  XR_OP_PRESS = 0,
  XR_OP_RELEASE = 1,
  XR_OP_MODAL = 2,
};

enum eXrActionFlag {
  /** Action depends on two sub-action paths (i.e. two-handed/bi-manual action). */
  XR_ACTION_BIMANUAL = (1 << 0),
};

enum eXrHapticFlag {
  /** Whether to apply haptics to corresponding user paths for an action and its haptic action. */
  XR_HAPTIC_MATCHUSERPATHS = (1 << 0),
  /**
   * Determines how haptics will be applied
   * ("repeat" is mutually exclusive with "press"/"release").
   */
  XR_HAPTIC_PRESS = (1 << 1),
  XR_HAPTIC_RELEASE = (1 << 2),
  XR_HAPTIC_REPEAT = (1 << 3),
};

/**
 * For axis-based inputs (thumbstick/trackpad/etc).
 * Determines the region for action execution (mutually exclusive per axis).
 */
enum eXrAxisFlag {
  XR_AXIS0_POS = (1 << 0),
  XR_AXIS0_NEG = (1 << 1),
  XR_AXIS1_POS = (1 << 2),
  XR_AXIS1_NEG = (1 << 3),
};

enum eXrPoseFlag {
  /* Pose represents controller grip/aim. */
  XR_POSE_GRIP = (1 << 0),
  XR_POSE_AIM = (1 << 1),
};

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

struct XrSessionSettings {
  /** Shading settings, struct shared with 3D-View so settings are the same. */
  struct View3DShading shading;

  float base_scale = 0;
  char _pad[3] = {};
  char base_pose_type = 0; /* #eXRSessionBasePoseType */
  /** Object to take the location and rotation as base position from. */
  Object *base_pose_object = nullptr;
  float base_pose_location[3] = {};
  float base_pose_angle = 0;

  /** View3D draw flags (V3D_OFSDRAW_NONE, V3D_OFSDRAW_SHOW_ANNOTATION, ...). */
  char draw_flags = 0;
  /** Draw style for controller visualization. */
  char controller_draw_style = 0;
  char _pad2[2] = {};

  /** Clipping distance. */
  float clip_start = 0, clip_end = 0;

  int flag = 0;

  /** Object type settings to apply to VR view (unlike shading, not shared with window 3D-View). */
  int object_type_exclude_viewport = 0;
  int object_type_exclude_select = 0;

  /** Fly speed. */
  float fly_speed = 0;

  /** View scale. */
  float view_scale = 1.0f;
};

/* -------------------------------------------------------------------- */

struct XrComponentPath {
  struct XrComponentPath *next = nullptr, *prev = nullptr;
  char path[/*XR_MAX_COMPONENT_PATH_LENGTH*/ 192] = "";
};

struct XrActionMapBinding {
  struct XrActionMapBinding *next = nullptr, *prev = nullptr;

  /** Unique name. */
  char name[/*MAX_NAME*/ 64] = "";

  /** OpenXR interaction profile path. */
  char profile[256] = "";
  /** OpenXR component paths. */
  ListBaseT<XrComponentPath> component_paths = {nullptr, nullptr};

  /** Input threshold/region. */
  float float_threshold = 0;
  short axis_flag = 0; /* eXrAxisFlag */
  char _pad[2] = {};

  /** Pose action properties. */
  float pose_location[3] = {};
  float pose_rotation[3] = {};
};

/* -------------------------------------------------------------------- */

struct XrUserPath {
  struct XrUserPath *next = nullptr, *prev = nullptr;
  char path[/*XR_MAX_USER_PATH_LENGTH*/ 64] = "";
};

struct XrActionMapItem {
  struct XrActionMapItem *next = nullptr, *prev = nullptr;

  /** Unique name. */
  char name[/*MAX_NAME*/ 64] = "";
  /** Type. */
  char type = 0; /** eXrActionType */
  char _pad[7] = {};

  /** OpenXR user paths. */
  ListBaseT<XrUserPath> user_paths = {nullptr, nullptr};

  /** Operator to be called on XR events. */
  char op[/*OP_MAX_TYPENAME*/ 64] = "";
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  IDProperty *op_properties = nullptr;
  /** RNA pointer to access properties. */
  struct PointerRNA *op_properties_ptr = nullptr;

  short op_flag = 0;     /* eXrOpFlag */
  short action_flag = 0; /* eXrActionFlag */
  short haptic_flag = 0; /* eXrHapticFlag */

  /** Pose action properties. */
  short pose_flag = 0; /* eXrPoseFlag */

  /** Haptic properties. */
  char haptic_name[/*MAX_NAME*/ 64] = "";
  float haptic_duration = 0;
  float haptic_frequency = 0;
  float haptic_amplitude = 0;

  short selbinding = 0;
  char _pad3[2] = {};
  ListBaseT<XrActionMapBinding> bindings = {nullptr, nullptr};
};

/* -------------------------------------------------------------------- */

struct XrActionMap {
  struct XrActionMap *next = nullptr, *prev = nullptr;

  /** Unique name. */
  char name[/*MAX_NAME*/ 64] = "";

  ListBaseT<XrActionMapItem> items = {nullptr, nullptr};
  short selitem = 0;
  char _pad[6] = {};
};

/* -------------------------------------------------------------------- */

}  // namespace blender
